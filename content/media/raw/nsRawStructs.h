/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla code.
 *
 * The Initial Developer of the Original Code is 
 *   Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Brad Lassey
 *  Kyle Huey <me@kylehuey.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#if !defined(nsRawStructs_h_)
#define nsRawStructs_h_

static const PRUint32 RAW_ID = 0x595556;

struct nsRawVideo_PRUint24 {
  operator PRUint32() const { return value[2] << 16 | value[1] << 8 | value[0]; }
  nsRawVideo_PRUint24& operator= (const PRUint32& rhs)
  { value[2] = (rhs & 0x00FF0000) >> 16;
    value[1] = (rhs & 0x0000FF00) >> 8;
    value[0] = (rhs & 0x000000FF);
    return *this; }
private:
  PRUint8 value[3];
};

struct nsRawPacketHeader {
  typedef nsRawVideo_PRUint24 PRUint24;
  PRUint8 packetID;
  PRUint24 codecID;
};

// This is Arc's draft from wiki.xiph.org/OggYUV
struct nsRawVideoHeader {
  typedef nsRawVideo_PRUint24 PRUint24;
  PRUint8 headerPacketID;          // Header Packet ID (always 0)
  PRUint24 codecID;                // Codec identifier (always "YUV")
  PRUint8 majorVersion;            // Version Major (breaks backwards compat)
  PRUint8 minorVersion;            // Version Minor (preserves backwards compat)
  PRUint16 options;                // Bit 1: Color (false = B/W)
                                   // Bits 2-4: Chroma Pixel Shape
                                   // Bit 5: 50% horizontal offset for Cr samples
                                   // Bit 6: 50% vertical ...
                                   // Bits 7-8: Chroma Blending
                                   // Bit 9: Packed (false = Planar)
                                   // Bit 10: Cr Staggered Horizontally
                                   // Bit 11: Cr Staggered Vertically
                                   // Bit 12: Unused (format is always little endian)
                                   // Bit 13: Interlaced (false = Progressive)
                                   // Bits 14-16: Interlace options (undefined)

  PRUint8 alphaChannelBpp;
  PRUint8 lumaChannelBpp;
  PRUint8 chromaChannelBpp;
  PRUint8 colorspace;

  PRUint24 frameWidth;
  PRUint24 frameHeight;
  PRUint24 aspectNumerator;
  PRUint24 aspectDenominator;

  PRUint32 framerateNumerator;
  PRUint32 framerateDenominator;
};

#endif // nsRawStructs_h_