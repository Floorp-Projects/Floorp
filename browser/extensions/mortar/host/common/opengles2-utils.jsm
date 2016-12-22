// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let GLES2Utils = {
  bytesPerElement: function(context, type) {
    switch (type) {
      case context.FLOAT:
      case context.INT:
      case context.UNSIGNED_INT:
        return 4;
      case context.SHORT:
      case context.UNSIGNED_SHORT:
      case context.UNSIGNED_SHORT_5_6_5:
      case context.UNSIGNED_SHORT_4_4_4_4:
      case context.UNSIGNED_SHORT_5_5_5_1:
        return 2;
      case context.BYTE:
      case context.UNSIGNED_BYTE:
        return 1;
      default:
        throw new Error("Don't know this type.");
    }
  },
  elementsPerGroup: function(context, format, type) {
    switch (type) {
      case context.UNSIGNED_SHORT_5_6_5:
      case context.UNSIGNED_SHORT_4_4_4_4:
      case context.UNSIGNED_SHORT_5_5_5_1:
        return 1;
      default:
        break;
    }

    switch (format) {
      case context.RGB:
        return 3;
      case context.LUMINANCE_ALPHA:
        return 2;
      case context.RGBA:
        return 4;
      case context.ALPHA:
      case context.LUMINANCE:
      case context.DEPTH_COMPONENT:
      case context.DEPTH_COMPONENT16:
        return 1;
      default:
        throw new Error("Don't know this format.");
    }
  },
  computeImageGroupSize: function(context, format, type) {
    return this.bytesPerElement(context, type) * this.elementsPerGroup(context, format, type);
  },
  computeImageDataSize: function(context, width, height, format, type) {
    const unpackAlignment = 4;

    let bytesPerGroup = this.computeImageGroupSize(context, format, type);
    let rowSize = width * bytesPerGroup;
    if (height == 1) {
      return rowSize;
    }

    let temp = rowSize + unpackAlignment - 1;
    let paddedRowSize = Math.floor(temp / unpackAlignment) * unpackAlignment;
    let sizeOfAllButLastRow = (height - 1) * paddedRowSize;
    return sizeOfAllButLastRow + rowSize;
  },
};

var EXPORTED_SYMBOLS = ["GLES2Utils"];
