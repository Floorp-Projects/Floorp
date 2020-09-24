/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/*
 * Parses an RTP packet
 * @param buffer an ArrayBuffer that contains the packet
 * @return { type: "rtp", header: {...}, payload: a DataView }
 */
var ParseRtpPacket = buffer => {
  // DataView.getFooInt returns big endian numbers by default
  let view = new DataView(buffer);

  // Standard Header Fields
  // https://tools.ietf.org/html/rfc3550#section-5.1
  //  0                   1                   2                   3
  //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |V=2|P|X|  CC   |M|     PT      |       sequence number         |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |                           timestamp                           |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  // |           synchronization source (SSRC) identifier            |
  // +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
  // |            contributing source (CSRC) identifiers             |
  // |                             ....                              |
  // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

  let header = {};
  let offset = 0;
  // Note that incrementing the offset happens as close to reading the data as
  // possible. This simplifies ensuring that the number of read bytes and the
  // offset increment match. Data may be manipulated between when the offset is
  // incremented and before the next read.
  let byte = view.getUint8(offset);
  offset++;
  // Version            2 Bit
  header.version = (0xc0 & byte) >> 6;
  // Padding            1 Bit
  header.padding = (0x30 & byte) >> 5;
  // Extension          1 Bit
  header.extensionsPresent = (0x10 & byte) >> 4 == 1;
  // CSRC count         4 Bit
  header.csrcCount = 0xf & byte;

  byte = view.getUint8(offset);
  offset++;
  // Marker             1 Bit
  header.marker = (0x80 & byte) >> 7;
  // Payload Type       7 Bit
  header.payloadType = 0x7f & byte;
  // Sequence Number   16 Bit
  header.sequenceNumber = view.getUint16(offset);
  offset += 2;
  // Timestamp         32 Bit
  header.timestamp = view.getUint32(offset);
  offset += 4;
  // SSRC              32 Bit
  header.ssrc = view.getUint32(offset);
  offset += 4;

  // CSRC              32 Bit
  header.csrcs = [];
  for (let c = 0; c < header.csrcCount; c++) {
    header.csrcs.push(view.getUint32(offset));
    offset += 4;
  }

  // Extensions
  header.extensions = [];
  header.extensionPaddingBytes = 0;
  header.extensionsTotalLength = 0;
  if (header.extensionsPresent) {
    // https://tools.ietf.org/html/rfc3550#section-5.3.1
    //  0                   1                   2                   3
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |      defined by profile       |           length              |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |                        header extension                       |
    // |                             ....                              |
    let addExtension = (id, len) =>
      header.extensions.push({
        id,
        data: new DataView(buffer, offset, len),
      });
    let extensionId = view.getUint16(offset);
    offset += 2;
    // len is in 32 bit units, not bytes
    header.extensionsTotalLength = view.getUint16(offset) * 4;
    offset += 2;
    // Check for https://tools.ietf.org/html/rfc5285
    if (extensionId != 0xbede) {
      // No rfc5285
      addExtension(extensionId, header.extensionsTotalLength);
      offset += header.extensionsTotalLength;
    } else {
      let expectedEnd = offset + header.extensionsTotalLength;
      while (offset < expectedEnd) {
        // We only support "one-byte" extension headers ATM
        // https://tools.ietf.org/html/rfc5285#section-4.2
        //  0
        //  0 1 2 3 4 5 6 7
        // +-+-+-+-+-+-+-+-+
        // |  ID   |  len  |
        // +-+-+-+-+-+-+-+-+
        byte = view.getUint8(offset);
        offset++;
        // Check for padding which can occur between extensions or at the end
        if (byte == 0) {
          header.extensionPaddingBytes++;
          continue;
        }
        let id = (byte & 0xf0) >> 4;
        // Check for the FORBIDDEN id (15), dun dun dun
        if (id == 15) {
          // Ignore bytes until until the end of extensions
          offset = expectedEnd;
          break;
        }
        // the length of the extention is len + 1
        let len = (byte & 0x0f) + 1;
        addExtension(id, len);
        offset += len;
      }
    }
  }
  return { type: "rtp", header, payload: new DataView(buffer, offset) };
};
