/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

use icu_properties::bidi::BidiClassAdapter;
use icu_properties::maps;

use unicode_bidi::level::Level;
use unicode_bidi::utf16;
use unicode_bidi::Direction;

use core::ops::Range;
use core::slice;

/// LevelRun type to be returned to C++.
/// 32-bit indexes (rather than usize) are sufficient here because Gecko works
/// with 32-bit indexes when collecting the text buffer for a paragraph.
#[repr(C)]
pub struct LevelRun {
    start: u32,
    length: u32,
    level: u8,
}

/// Bidi object to be exposed to Gecko via FFI.
pub struct UnicodeBidi<'a> {
    paragraph_info: utf16::ParagraphBidiInfo<'a>,
    resolved: Option<(Vec<Level>, Vec<Range<usize>>)>,
}

impl UnicodeBidi<'_> {
    /// Create a new UnicodeBidi object representing the given text. This creates
    /// the unicode-bidi ParagraphBidiInfo struct, and will cache the resolved
    /// levels and visual-runs array once created.
    /// The caller is responsible to ensure the text buffer remains valid
    /// as long as the UnicodeBidi object exists.
    fn new<'a>(text: *const u16, length: usize, level: u8) -> Box<Self> {
        let text = unsafe { slice::from_raw_parts(text, length) };
        let level = if let Ok(level) = Level::new(level) {
            Some(level)
        } else {
            None
        };
        let adapter = BidiClassAdapter::new(maps::bidi_class());
        Box::new(UnicodeBidi {
            paragraph_info: utf16::ParagraphBidiInfo::new_with_data_source(
                &adapter, text, level,
            ),
            resolved: None,
        })
    }

    #[inline]
    fn resolved(&mut self) -> &(Vec<Level>, Vec<Range<usize>>) {
        if self.resolved.is_none() {
            let len = self.paragraph_info.text.len();
            self.resolved = Some(self.paragraph_info.visual_runs(0..len));
        }
        self.resolved.as_ref().unwrap()
    }
}

/// Create a new UnicodeBidi object for the given text.
/// NOTE that the text buffer must remain valid for the lifetime of this object!
#[no_mangle]
pub extern "C" fn bidi_new<'a>(text: *const u16, length: usize, level: u8) -> *mut UnicodeBidi<'a> {
    Box::into_raw(UnicodeBidi::<'a>::new(text, length, level))
}

/// Destroy the Bidi object.
#[no_mangle]
pub extern "C" fn bidi_destroy(bidi: *mut UnicodeBidi) {
    if bidi.is_null() {
        return;
    }
    let _ = unsafe { Box::from_raw(bidi) };
}

/// Get the length of the text covered by the Bidi object.
#[no_mangle]
pub extern "C" fn bidi_get_length(bidi: &UnicodeBidi) -> i32 {
    bidi.paragraph_info.text.len().try_into().unwrap()
}

/// Get the paragraph direction: LTR=1, RTL=-1, mixed=0.
#[no_mangle]
pub extern "C" fn bidi_get_direction(bidi: &UnicodeBidi) -> i8 {
    match bidi.paragraph_info.direction() {
        Direction::Mixed => 0,
        Direction::Ltr => 1,
        Direction::Rtl => -1,
    }
}

/// Get the paragraph level.
#[no_mangle]
pub extern "C" fn bidi_get_paragraph_level(bidi: &UnicodeBidi) -> u8 {
    bidi.paragraph_info.paragraph_level.into()
}

/// Get the number of runs present.
#[no_mangle]
pub extern "C" fn bidi_count_runs(bidi: &mut UnicodeBidi) -> i32 {
    if bidi.paragraph_info.text.is_empty() {
        return 0;
    }
    bidi.resolved().1.len().try_into().unwrap()
}

/// Get a pointer to the Levels array. The resulting pointer is valid only as long as
/// the UnicodeBidi object exists!
#[no_mangle]
pub extern "C" fn bidi_get_levels(bidi: &mut UnicodeBidi) -> *const Level {
    bidi.resolved().0.as_ptr()
}

/// Get the extent of the run at the given index in the visual runs array.
/// This would panic!() if run_index is out of range (see bidi_count_runs),
/// or if the run's start or length exceeds u32::MAX (which cannot happen
/// because Gecko can't create such a huge text buffer).
#[no_mangle]
pub extern "C" fn bidi_get_visual_run(bidi: &mut UnicodeBidi, run_index: u32) -> LevelRun {
    let level_runs = &bidi.resolved().1;
    let start = level_runs[run_index as usize].start;
    let length = level_runs[run_index as usize].end - start;
    LevelRun {
        start: start.try_into().unwrap(),
        length: length.try_into().unwrap(),
        level: bidi.resolved().0[start].into(),
    }
}

/// Return index map showing the result of reordering using the given levels array.
/// (This is a generic helper that does not use a UnicodeBidi object, it just takes an
/// arbitrary array of levels.)
#[no_mangle]
pub extern "C" fn bidi_reorder_visual(levels: *const u8, length: usize, index_map: *mut i32) {
    let levels = unsafe { slice::from_raw_parts(levels as *const Level, length) };
    let result = unsafe { slice::from_raw_parts_mut(index_map, length) };
    let reordered = utf16::BidiInfo::reorder_visual(levels);
    for i in 0..length {
        result[i] = reordered[i].try_into().unwrap();
    }
}

/// Get the base direction for the given text, returning 1 for LTR, -1 for RTL,
/// and 0 for neutral. If first_paragraph is true, only the first paragraph will be considered;
/// if false, subsequent paragraphs may be considered until a non-neutral character is found.
#[no_mangle]
pub extern "C" fn bidi_get_base_direction(
    text: *const u16,
    length: usize,
    first_paragraph: bool,
) -> i8 {
    let text = unsafe { slice::from_raw_parts(text, length) };
    let adapter = BidiClassAdapter::new(maps::bidi_class());
    let direction = if first_paragraph {
        unicode_bidi::get_base_direction_with_data_source(&adapter, text)
    } else {
        unicode_bidi::get_base_direction_full_with_data_source(&adapter, text)
    };
    match direction {
        Direction::Mixed => 0,
        Direction::Ltr => 1,
        Direction::Rtl => -1,
    }
}
