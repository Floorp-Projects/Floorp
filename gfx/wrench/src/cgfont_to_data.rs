/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use byteorder::{BigEndian, ByteOrder, ReadBytesExt, WriteBytesExt};
use core_foundation::data::CFData;
use core_graphics::font::CGFont;
use std;
use std::io::Cursor;
use std::io::Read;
use std::io::Write;


fn calc_table_checksum<D: Read>(mut data: D) -> u32 {
    let mut sum: u32 = 0;

    while let Ok(x) = data.read_u32::<BigEndian>() {
        sum = sum.wrapping_add(x);
    }
    // read the remaining bytes
    let mut buf: [u8; 4] = [0; 4];
    data.read(&mut buf).unwrap();
    // if there was nothing left in buf we'll just read a 0
    // which won't affect the checksum
    sum = sum.wrapping_add(BigEndian::read_u32(&buf));
    sum
}

fn max_pow_2_less_than(a: u16) -> u16 {
    let x = 1;
    let mut shift = 0;
    while a > (x << (shift + 1)) {
        shift += 1;
    }
    shift
}

struct TableRecord {
    tag: u32,
    checksum: u32,
    offset: u32,
    length: u32,
    data: CFData
}

const CFF_TAG: u32 = 0x43464620; // 'CFF '
const HEAD_TAG: u32 = 0x68656164; // 'head'
const OTTO_TAG: u32 = 0x4f54544f; // 'OTTO'
const TRUE_TAG: u32 = 0x00010000;

pub fn font_to_data(font: CGFont) -> Result<Vec<u8>, std::io::Error> {
    // We'll reconstruct a TTF font from the tables we can get from the CGFont
    let mut cff = false;
    let tags = font.copy_table_tags();
    let count = tags.len() as u16;

    let mut records = Vec::new();
    let mut offset: u32 = 0;
    offset += 4 * 3;
    offset += 4 * 4 * (count as u32);
    for tag_ref in tags.iter() {
        let tag = *tag_ref;
        if tag == CFF_TAG {
            cff = true;
        }
        let data = font.copy_table_for_tag(tag).unwrap();
        let length = data.len() as u32;
        let checksum;
        if tag == HEAD_TAG {
            // we need to skip the checksum field
            checksum = calc_table_checksum(&data.bytes()[0..2])
                .wrapping_add(calc_table_checksum(&data.bytes()[3..]))
        } else {
            checksum = calc_table_checksum(data.bytes());
        }
        records.push(TableRecord { tag, offset, data, length, checksum } );
        offset += length;
        // 32 bit align the tables
        offset = (offset + 3) & !3;
    }

    let mut buf = Vec::new();
    if cff {
        buf.write_u32::<BigEndian>(OTTO_TAG)?;
    } else {
        buf.write_u32::<BigEndian>(TRUE_TAG)?;
    }

    buf.write_u16::<BigEndian>(count)?;
    buf.write_u16::<BigEndian>((1 << max_pow_2_less_than(count)) * 16)?;
    buf.write_u16::<BigEndian>(max_pow_2_less_than(count))?;
    buf.write_u16::<BigEndian>(count * 16 - ((1 << max_pow_2_less_than(count)) * 16))?;

    // write table record entries
    for r in &records {
        buf.write_u32::<BigEndian>(r.tag)?;
        buf.write_u32::<BigEndian>(r.checksum)?;
        buf.write_u32::<BigEndian>(r.offset)?;
        buf.write_u32::<BigEndian>(r.length)?;
    }

    // write tables
    let mut check_sum_adjustment_offset = 0;
    for r in &records {
        if r.tag == 0x68656164 {
            check_sum_adjustment_offset = buf.len() + 2 * 4;
        }
        buf.write(r.data.bytes())?;
        // 32 bit align the tables
        while buf.len() & 3 != 0 {
            buf.push(0);
        }
    }

    let mut c = Cursor::new(buf);
    c.set_position(check_sum_adjustment_offset as u64);
    // clear the checksumAdjust field before checksumming the whole font
    c.write_u32::<BigEndian>(0)?;
    let sum = 0xb1b0afba_u32.wrapping_sub(calc_table_checksum(&c.get_mut()[..]));
    // set checkSumAdjust to the computed checksum
    c.set_position(check_sum_adjustment_offset as u64);
    c.write_u32::<BigEndian>(sum)?;

    Ok(c.into_inner())
}
