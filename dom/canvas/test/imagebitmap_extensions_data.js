class Image {
  constructor(channelCount, format, ArrayType) {
    this.channelCount = channelCount;
    this.format = format;
    this.ArrayType = ArrayType;
    this.pixelLayout = [];
    this.buffer = undefined;
    this.data = undefined;
  };
};

class TypedSimpleImage extends Image {
  constructor(width, height, channelCount, format, ArrayType) {
    super(channelCount, format, ArrayType);
    this.width = width;
    this.height = height;
    this.stride = this.width * this.channelCount * this.ArrayType.BYTES_PER_ELEMENT;
    this.bufferLength = this.height * this.stride;
    this.buffer = new ArrayBuffer(this.bufferLength);
    this.data = new this.ArrayType(this.buffer, 0, this.height * this.width * this.channelCount);

    // initialize pixel layout
    for (var c = 0; c < this.channelCount; ++c) {
      this.pixelLayout.push({offset:c * this.ArrayType.BYTES_PER_ELEMENT,
                             width:this.width,
                             height:this.height,
                             dataType:"uint8",
                             stride:this.stride,
                             skip:(this.channelCount - 1)});
    }
  };

  getPixelValue(i, j, c) {
    var dataChannelLayout = this.pixelLayout[c];
    return this.data[i * this.width * this.channelCount + j * this.channelCount + c];
  }
};

class Uint8SimpleImage extends TypedSimpleImage {
  constructor(width, height, channelCount, format) {
    super(width, height, channelCount, format, Uint8ClampedArray);
  };
};

class Uint16SimpleImage extends TypedSimpleImage {
  constructor(width, height, channelCount, format) {
    super(width, height, channelCount, format, Uint16Array);
  };
};

class FloatSimpleImage extends TypedSimpleImage {
  constructor(width, height, channelCount, format) {
    super(width, height, channelCount, format, Float32Array);
  };
};

class DoubleSimpleImage extends TypedSimpleImage {
  constructor(width, height, channelCount, format) {
    super(width, height, channelCount, format, Float64Array);
  };
};

class RGBA32Data extends Uint8SimpleImage {
  constructor() {
    super(3, 3, 4, "RGBA32");

    var i = 0;
    this.data[i + 0] = 0;   this.data[i + 1] = 0;   this.data[i + 2] = 0;   this.data[i + 3] = 255;
    this.data[i + 4] = 255; this.data[i + 5] = 0;   this.data[i + 6] = 0;   this.data[i + 7] = 255;
    this.data[i + 8] = 0;   this.data[i + 9] = 255; this.data[i + 10] = 0;  this.data[i + 11] = 255;

    i += this.stride;
    this.data[i + 0] = 0;   this.data[i + 1] = 0;   this.data[i + 2] = 255;  this.data[i + 3] = 255;
    this.data[i + 4] = 255; this.data[i + 5] = 255; this.data[i + 6] = 0;    this.data[i + 7] = 255;
    this.data[i + 8] = 0;   this.data[i + 9] = 255; this.data[i + 10] = 255; this.data[i + 11] = 255;

    i += this.stride;
    this.data[i + 0] = 255;  this.data[i + 1] = 0;   this.data[i + 2] = 255;  this.data[i + 3] = 255;
    this.data[i + 4] = 255;  this.data[i + 5] = 255; this.data[i + 6] = 255;  this.data[i + 7] = 255;
    this.data[i + 8] = 128;  this.data[i + 9] = 128; this.data[i + 10] = 128; this.data[i + 11] = 255;
  };
};

class BGRA32Data extends Uint8SimpleImage {
  constructor() {
    super(3, 3, 4, "BGRA32");

    var i = 0;
    this.data[i + 2] = 0;   this.data[i + 1] = 0;   this.data[i + 0] = 0;   this.data[i + 3] = 255;
    this.data[i + 6] = 255; this.data[i + 5] = 0;   this.data[i + 4] = 0;   this.data[i + 7] = 255;
    this.data[i + 10] = 0;  this.data[i + 9] = 255; this.data[i + 8] = 0;   this.data[i + 11] = 255;

    i += this.stride;
    this.data[i + 2] = 0;   this.data[i + 1] = 0;   this.data[i + 0] = 255;  this.data[i + 3] = 255;
    this.data[i + 6] = 255; this.data[i + 5] = 255; this.data[i + 4] = 0;    this.data[i + 7] = 255;
    this.data[i + 10] = 0;  this.data[i + 9] = 255; this.data[i + 8] = 255;  this.data[i + 11] = 255;

    i += this.stride;
    this.data[i + 2] = 255;  this.data[i + 1] = 0;   this.data[i + 0] = 255; this.data[i + 3] = 255;
    this.data[i + 6] = 255;  this.data[i + 5] = 255; this.data[i + 4] = 255; this.data[i + 7] = 255;
    this.data[i + 10] = 128; this.data[i + 9] = 128; this.data[i + 8] = 128; this.data[i + 11] = 255;
  };
};

class RGB24Data extends Uint8SimpleImage {
  constructor() {
    super(3, 3, 3, "RGB24");

    var i = 0;
    this.data[i + 0] = 0;   this.data[i + 1] = 0;   this.data[i + 2] = 0;
    this.data[i + 3] = 255; this.data[i + 4] = 0;   this.data[i + 5] = 0;
    this.data[i + 6] = 0;   this.data[i + 7] = 255; this.data[i + 8] = 0;

    i += this.stride;
    this.data[i + 0] = 0;   this.data[i + 1] = 0;   this.data[i + 2] = 255;
    this.data[i + 3] = 255; this.data[i + 4] = 255; this.data[i + 5] = 0;
    this.data[i + 6] = 0;   this.data[i + 7] = 255; this.data[i + 8] = 255;

    i += this.stride;
    this.data[i + 0] = 255;  this.data[i + 1] = 0;   this.data[i + 2] = 255;
    this.data[i + 3] = 255;  this.data[i + 4] = 255; this.data[i + 5] = 255;
    this.data[i + 6] = 128;  this.data[i + 7] = 128; this.data[i + 8] = 128;
  };
};

class BGR24Data extends Uint8SimpleImage {
  constructor() {
    super(3, 3, 3, "BGR24");

    var i = 0;
    this.data[i + 2] = 0;   this.data[i + 1] = 0;   this.data[i + 0] = 0;
    this.data[i + 5] = 255; this.data[i + 4] = 0;   this.data[i + 3] = 0;
    this.data[i + 8] = 0;   this.data[i + 7] = 255; this.data[i + 6] = 0;

    i += this.stride;
    this.data[i + 2] = 0;   this.data[i + 1] = 0;   this.data[i + 0] = 255;
    this.data[i + 5] = 255; this.data[i + 4] = 255; this.data[i + 3] = 0;
    this.data[i + 8] = 0;   this.data[i + 7] = 255; this.data[i + 6] = 255;

    i += this.stride;
    this.data[i + 2] = 255; this.data[i + 1] = 0;   this.data[i + 0] = 255;
    this.data[i + 5] = 255; this.data[i + 4] = 255; this.data[i + 3] = 255;
    this.data[i + 8] = 128; this.data[i + 7] = 128; this.data[i + 6] = 128;
  };
};

class Gray8Data extends Uint8SimpleImage {
  constructor() {
    super(3, 3, 1, "GRAY8");

    var i = 0;
    this.data[i + 0] = 0;   this.data[i + 1] = 76;   this.data[i + 2] = 149;

    i += this.stride;
    this.data[i + 0] = 29;   this.data[i + 1] = 225;   this.data[i + 2] = 178;

    i += this.stride;
    this.data[i + 0] = 105; this.data[i + 1] = 255;   this.data[i + 2] = 127;
  };
};

class RGBA32DataFromYUV444PData extends Uint8SimpleImage {
  constructor(redIndex, greenIndex, blueIndex, alphaIndex) {

    // Get the exact format.
    var channelCount_ = !!alphaIndex ? 4 : 3;
    var format_;
    if (redIndex == 0) {
      if (channelCount_ == 3) {
        format_ = "RGBA32";
      } else  {
        format_ = "RGB24";
      }
    } else if (redIndex == 2) {
      if (channelCount_ == 3) {
        format_ = "BGRA32";
      } else  {
        format_ = "BGR24";
      }
    } else {
      return undefined;
    }

    // Call parent's consturctor.
    super(3, 3, channelCount_, format_);

    // Calculate parameters for setting data.
    var rIndex = redIndex;        // 0 or 2
    var gIndex = 1;
    var bIndex = redIndex ^ 2;    // 0 or 2
    var aIndex = alphaIndex || 0; // If alphaIndex == undefined --> aIndex = 0;
    var shift0 = 0;
    var shift1 = channelCount_;
    var shift2 = channelCount_ * 2;

    // Set the data.
    var i = 0;
    this.data[i + aIndex + shift0] = 255;   this.data[i + rIndex + shift0] = 0;   this.data[i + gIndex + shift0] = 0;   this.data[i + bIndex + shift0] = 0;
    this.data[i + aIndex + shift1] = 255;   this.data[i + rIndex + shift1] = 254; this.data[i + gIndex + shift1] = 0;   this.data[i + bIndex + shift1] = 0;
    this.data[i + aIndex + shift2] = 255;   this.data[i + rIndex + shift2] = 0;   this.data[i + gIndex + shift2] = 253; this.data[i + bIndex + shift2] = 1;

    i += this.stride;
    this.data[i + aIndex + shift0] = 255;   this.data[i + rIndex + shift0] = 0;   this.data[i + gIndex + shift0] = 0;   this.data[i + bIndex + shift0] = 251;
    this.data[i + aIndex + shift1] = 255;   this.data[i + rIndex + shift1] = 253; this.data[i + gIndex + shift1] = 253; this.data[i + bIndex + shift1] = 2;
    this.data[i + aIndex + shift2] = 255;   this.data[i + rIndex + shift2] = 0;   this.data[i + gIndex + shift2] = 253; this.data[i + bIndex + shift2] = 252;

    i += this.stride;
    this.data[i + aIndex + shift0] = 255;   this.data[i + rIndex + shift0] = 255; this.data[i + gIndex + shift0] = 0;   this.data[i + bIndex + shift0] = 252;
    this.data[i + aIndex + shift1] = 255;   this.data[i + rIndex + shift1] = 253; this.data[i + gIndex + shift1] = 253; this.data[i + bIndex + shift1] = 253;
    this.data[i + aIndex + shift2] = 255;   this.data[i + rIndex + shift2] = 127; this.data[i + gIndex + shift2] = 127; this.data[i + bIndex + shift2] = 127;
  };
};

class RGBA32DataFromYUV422PData extends Uint8SimpleImage {
  constructor(redIndex, greenIndex, blueIndex, alphaIndex) {

    // Get the exact format.
    var channelCount_ = !!alphaIndex ? 4 : 3;
    var format_;
    if (redIndex == 0) {
      if (channelCount_ == 3) {
        format_ = "RGBA32";
      } else  {
        format_ = "RGB24";
      }
    } else if (redIndex == 2) {
      if (channelCount_ == 3) {
        format_ = "BGRA32";
      } else  {
        format_ = "BGR24";
      }
    } else {
      return undefined;
    }

    // Call parent's consturctor.
    super(3, 3, channelCount_, format_);

    // Calculate parameters for setting data.
    var rIndex = redIndex;        // 0 or 2
    var gIndex = 1;
    var bIndex = redIndex ^ 2;    // 0 or 2
    var aIndex = alphaIndex || 0; // If alphaIndex == undefined --> aIndex = 0;
    var shift0 = 0;
    var shift1 = channelCount_;
    var shift2 = channelCount_ * 2;

    // Set the data.
    var i = 0;
    this.data[i + aIndex + shift0] = 255;   this.data[i + rIndex + shift0] = 89;  this.data[i + gIndex + shift0] = 0;   this.data[i + bIndex + shift0] = 0;
    this.data[i + aIndex + shift1] = 255;   this.data[i + rIndex + shift1] = 165; this.data[i + gIndex + shift1] = 38;  this.data[i + bIndex + shift1] = 38;
    this.data[i + aIndex + shift2] = 255;   this.data[i + rIndex + shift2] = 0;   this.data[i + gIndex + shift2] = 253; this.data[i + bIndex + shift2] = 1;

    i += this.stride;
    this.data[i + aIndex + shift0] = 255;   this.data[i + rIndex + shift0] = 28;  this.data[i + gIndex + shift0] = 28;  this.data[i + bIndex + shift0] = 28;
    this.data[i + aIndex + shift1] = 255;   this.data[i + rIndex + shift1] = 224; this.data[i + gIndex + shift1] = 224; this.data[i + bIndex + shift1] = 224;
    this.data[i + aIndex + shift2] = 255;   this.data[i + rIndex + shift2] = 0;   this.data[i + gIndex + shift2] = 253; this.data[i + bIndex + shift2] = 252;

    i += this.stride;
    this.data[i + aIndex + shift0] = 255;   this.data[i + rIndex + shift0] = 180; this.data[i + gIndex + shift0] = 52;  this.data[i + bIndex + shift0] = 178;
    this.data[i + aIndex + shift1] = 255;   this.data[i + rIndex + shift1] = 255; this.data[i + gIndex + shift1] = 200; this.data[i + bIndex + shift1] = 255;
    this.data[i + aIndex + shift2] = 255;   this.data[i + rIndex + shift2] = 127; this.data[i + gIndex + shift2] = 127; this.data[i + bIndex + shift2] = 127;
  };
};

class RGBA32DataFromYUV420PData extends Uint8SimpleImage {
  constructor(redIndex, greenIndex, blueIndex, alphaIndex) {

    // Get the exact format.
    var channelCount_ = !!alphaIndex ? 4 : 3;
    var format_;
    if (redIndex == 0) {
      if (channelCount_ == 3) {
        format_ = "RGBA32";
      } else  {
        format_ = "RGB24";
      }
    } else if (redIndex == 2) {
      if (channelCount_ == 3) {
        format_ = "BGRA32";
      } else  {
        format_ = "BGR24";
      }
    } else {
      return undefined;
    }

    // Call parent's consturctor.
    super(3, 3, channelCount_, format_);

    // Calculate parameters for setting data.
    var rIndex = redIndex;        // 0 or 2
    var gIndex = 1;
    var bIndex = redIndex ^ 2;    // 0 or 2
    var aIndex = alphaIndex || 0; // If alphaIndex == undefined --> aIndex = 0;
    var shift0 = 0;
    var shift1 = channelCount_;
    var shift2 = channelCount_ * 2;

    // Set the data.
    var i = 0;
    this.data[i + aIndex + shift0] = 255;   this.data[i + rIndex + shift0] = 44;  this.data[i + gIndex + shift0] = 0;   this.data[i + bIndex + shift0] = 0;
    this.data[i + aIndex + shift1] = 255;   this.data[i + rIndex + shift1] = 120; this.data[i + gIndex + shift1] = 57;  this.data[i + bIndex + shift1] = 58;
    this.data[i + aIndex + shift2] = 255;   this.data[i + rIndex + shift2] = 0;   this.data[i + gIndex + shift2] = 238; this.data[i + bIndex + shift2] = 112;

    i += this.stride;
    this.data[i + aIndex + shift0] = 255;   this.data[i + rIndex + shift0] = 73;  this.data[i + gIndex + shift0] = 9;   this.data[i + bIndex + shift0] = 11;
    this.data[i + aIndex + shift1] = 255;   this.data[i + rIndex + shift1] = 255; this.data[i + gIndex + shift1] = 205; this.data[i + bIndex + shift1] = 206;
    this.data[i + aIndex + shift2] = 255;   this.data[i + rIndex + shift2] = 12;  this.data[i + gIndex + shift2] = 255; this.data[i + bIndex + shift2] = 141;

    i += this.stride;
    this.data[i + aIndex + shift0] = 255;   this.data[i + rIndex + shift0] = 180; this.data[i + gIndex + shift0] = 52;  this.data[i + bIndex + shift0] = 178;
    this.data[i + aIndex + shift1] = 255;   this.data[i + rIndex + shift1] = 255; this.data[i + gIndex + shift1] = 200; this.data[i + bIndex + shift1] = 255;
    this.data[i + aIndex + shift2] = 255;   this.data[i + rIndex + shift2] = 127; this.data[i + gIndex + shift2] = 127; this.data[i + bIndex + shift2] = 127;
  };
};

class Gray8DataFromYUVData extends Uint8SimpleImage {
  constructor() {
    super(3, 3, 1, "GRAY8");

    var i = 0;
    this.data[i + 0] = 16;  this.data[i + 1] = 82;   this.data[i + 2] = 144;

    i += this.stride;
    this.data[i + 0] = 41;  this.data[i + 1] = 210;  this.data[i + 2] = 169;

    i += this.stride;
    this.data[i + 0] = 107; this.data[i + 1] = 235;  this.data[i + 2] = 126;
  };
};

class YUVImage extends Image {
  constructor(yWidth, yHeight, uWidth, uHeight, vWidth, vHeight, format) {
    super(3, format, Uint8ClampedArray);
    this.yWidth = yWidth;
    this.yHeight = yHeight;
    this.yStride = this.yWidth * this.ArrayType.BYTES_PER_ELEMENT;
    this.uWidth = uWidth;
    this.uHeight = uHeight;
    this.uStride = this.uWidth * this.ArrayType.BYTES_PER_ELEMENT;
    this.vWidth = vWidth;
    this.vHeight = vHeight;
    this.vStride = this.vWidth * this.ArrayType.BYTES_PER_ELEMENT;
    this.bufferLength = this.yHeight * this.yStride +
                        this.uHeight * this.uStride +
                        this.vHeight * this.vStride
    this.buffer = new ArrayBuffer(this.bufferLength);
    this.data = new this.ArrayType(this.buffer, 0, this.bufferLength);
    this.yData = new this.ArrayType(this.buffer, 0, this.yHeight * this.yStride);
    this.uData = new this.ArrayType(this.buffer,
                                    this.yHeight * this.yStride,
                                    this.uHeight * this.uStride);
    this.vData = new this.ArrayType(this.buffer,
                                    this.yHeight * this.yStride + this.uHeight * this.uStride,
                                    this.vHeight * this.vStride);

    // Initialize pixel layout.
    // y channel.
    this.pixelLayout.push({offset:0,
                           width:this.yWidth,
                           height:this.yHeight,
                           dataType:"uint8",
                           stride:this.yStride,
                           skip:0});

    // u channel.
    this.pixelLayout.push({offset:(this.yHeight * this.yStride),
                           width:this.uWidth,
                           height:this.uHeight,
                           dataType:"uint8",
                           stride:this.uStride,
                           skip:0});

    // v channel.
    this.pixelLayout.push({offset:(this.yHeight * this.yStride + this.uHeight * this.uStride),
                           width:this.vWidth,
                           height:this.vHeight,
                           dataType:"uint8",
                           stride:this.vStride,
                           skip:0});
  };

  getPixelValue(i, j, c) {
    var dataChannelLayout = this.pixelLayout[c];
    return this.data[dataChannelLayout.offset + i * dataChannelLayout.stride + j * (dataChannelLayout.skip + 1)];
  }
};

class YUV444PData extends YUVImage {
  constructor() {
    super(3, 3, 3, 3, 3, 3, "YUV444P");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    this.uData[0] = 128; this.uData[1] = 90;  this.uData[2] = 54;
    this.uData[3] = 240; this.uData[4] = 16;  this.uData[5] = 166;
    this.uData[6] = 202; this.uData[7] = 128; this.uData[8] = 128;

    this.vData[0] = 128; this.vData[1] = 240; this.vData[2] = 34;
    this.vData[3] = 110; this.vData[4] = 146; this.vData[5] = 16;
    this.vData[6] = 222; this.vData[7] = 128; this.vData[8] = 128;
  }
};

class YUV444PDataFromYUV422PData extends YUVImage {
  constructor() {
    super(3, 3, 3, 3, 3, 3, "YUV444P");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    this.uData[0] = 108; this.uData[1] = 81;  this.uData[2] = 53;
    this.uData[3] = 126; this.uData[4] = 112; this.uData[5] = 98;
    this.uData[6] = 144; this.uData[7] = 144; this.uData[8] = 144;

    this.vData[0] = 182; this.vData[1] = 108; this.vData[2] = 33;
    this.vData[3] = 166; this.vData[4] = 109; this.vData[5] = 51;
    this.vData[6] = 150; this.vData[7] = 110; this.vData[8] = 70;
  }
};

class YUV444PDataFromYUV420PData extends YUVImage {
  constructor() {
    super(3, 3, 3, 3, 3, 3, "YUV444P");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    this.uData[0] = 118; this.uData[1] = 113; this.uData[2] = 109;
    this.uData[3] = 140; this.uData[4] = 128; this.uData[5] = 117;
    this.uData[6] = 162; this.uData[7] = 144; this.uData[8] = 126;

    this.vData[0] = 154; this.vData[1] = 90;  this.vData[2] = 24;
    this.vData[3] = 163; this.vData[4] = 119; this.vData[5] = 75;
    this.vData[6] = 172; this.vData[7] = 149; this.vData[8] = 126;
  }
};

class YUV422PData extends YUVImage {
  constructor() {
    super(3, 3, 2, 3, 2, 3, "YUV422P");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    this.uData[0] = 109; this.uData[1] = 54;
    this.uData[2] = 128; this.uData[3] = 166;
    this.uData[4] = 165; this.uData[5] = 128;

    this.vData[0] = 184; this.vData[1] = 34;
    this.vData[2] = 128; this.vData[3] = 16;
    this.vData[4] = 175; this.vData[5] = 128;
  }
};

class YUV422PDataFromYUV444PData extends YUVImage {
  constructor() {
    super(3, 3, 2, 3, 2, 3, "YUV422P");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    this.uData[0] = 133; this.uData[1] = 78;
    this.uData[2] = 164; this.uData[3] = 109;
    this.uData[4] = 180; this.uData[5] = 125;

    this.vData[0] = 145; this.vData[1] = 74;
    this.vData[2] = 165; this.vData[3] = 95;
    this.vData[4] = 175; this.vData[5] = 105;
  }
};

class YUV422PDataFromYUV420PData extends YUVImage {
  constructor() {
    super(3, 3, 2, 3, 2, 3, "YUV422P");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    this.uData[0] = 119; this.uData[1] = 110;
    this.uData[2] = 149; this.uData[3] = 121;
    this.uData[4] = 164; this.uData[5] = 127;

    this.vData[0] = 156; this.vData[1] = 25;
    this.vData[2] = 168; this.vData[3] = 93;
    this.vData[4] = 174; this.vData[5] = 127;
  }
};

class YUV420PData extends YUVImage {
  constructor() {
    super(3, 3, 2, 2, 2, 2, "YUV420P");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    this.uData[0] = 119; this.uData[1] = 110;
    this.uData[2] = 165; this.uData[3] = 128;

    this.vData[0] = 156; this.vData[1] = 25;
    this.vData[2] = 175; this.vData[3] = 128;
  }
};

class YUV420PDataFromYUV444PData extends YUVImage {
  constructor() {
    super(3, 3, 2, 2, 2, 2, "YUV420P");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    this.uData[0] = 133; this.uData[1] = 78;
    this.uData[2] = 181; this.uData[3] = 126;

    this.vData[0] = 145; this.vData[1] = 74;
    this.vData[2] = 176; this.vData[3] = 106;
  }
};

class YUV420PDataFromYUV422PData extends YUVImage {
  constructor() {
    super(3, 3, 2, 2, 2, 2, "YUV420P");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    this.uData[0] = 109; this.uData[1] = 54;
    this.uData[2] = 147; this.uData[3] = 147;

    this.vData[0] = 184; this.vData[1] = 34;
    this.vData[2] = 152; this.vData[3] = 72;
  }
};

class NVImage extends Image {
  constructor(yWidth, yHeight, uvWidth, uvHeight, format) {
    super(3, format, Uint8ClampedArray);
    this.yWidth = yWidth;
    this.yHeight = yHeight;
    this.yStride = this.yWidth * this.ArrayType.BYTES_PER_ELEMENT;
    this.uvWidth = uvWidth;
    this.uvHeight = uvHeight;
    this.uvStride = this.uvWidth * 2 * this.ArrayType.BYTES_PER_ELEMENT;
    this.bufferLength = this.yHeight * this.yStride + this.uvHeight * this.uvStride;
    this.buffer = new ArrayBuffer(this.bufferLength);
    this.data = new this.ArrayType(this.buffer, 0, this.bufferLength);
    this.yData = new this.ArrayType(this.buffer, 0, this.yHeight * this.yStride);
    this.uvData = new this.ArrayType(this.buffer,
                                     this.yHeight * this.yStride,
                                     this.uvHeight * this.uvStride);

    // Initialize pixel layout.
    // y channel.
    this.pixelLayout.push({offset:0,
                           width:this.yWidth,
                           height:this.yHeight,
                           dataType:"uint8",
                           stride:this.yStride,
                           skip:0});
    // v channel.
    this.pixelLayout.push({offset:(this.yHeight * this.yStride),
                           width:this.uvWidth,
                           height:this.uvHeight,
                           dataType:"uint8",
                           stride:this.uvStride,
                           skip:1});

    // u channel.
    this.pixelLayout.push({offset:(this.yHeight * this.yStride + this.ArrayType.BYTES_PER_ELEMENT),
                           width:this.uvWidth,
                           height:this.uvHeight,
                           dataType:"uint8",
                           stride:this.uvStride,
                           skip:1});
  };

  getPixelValue(i, j, c) {
    var dataChannelLayout = this.pixelLayout[c];
    return this.data[dataChannelLayout.offset + i * dataChannelLayout.stride + j * (dataChannelLayout.skip + 1)];
  }
};

class NV12Data extends NVImage {
  constructor() {
    super(3, 3, 2, 2, "YUV420SP_NV12");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    // U
    this.uvData[0] = 119;
    this.uvData[2] = 110;
    this.uvData[4] = 165;
    this.uvData[6] = 128;

    // V
    this.uvData[1] = 156;
    this.uvData[3] = 25;
    this.uvData[5] = 175;
    this.uvData[7] = 128;
  };
};

class NV12DataFromYUV444PData extends NVImage {
  constructor() {
    super(3, 3, 2, 2, "YUV420SP_NV12");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    // U
    this.uvData[0] = 133;
    this.uvData[2] = 78;
    this.uvData[4] = 181;
    this.uvData[6] = 126;

    // V
    this.uvData[1] = 145;
    this.uvData[3] = 74;
    this.uvData[5] = 176;
    this.uvData[7] = 106;
  };
};

class NV12DataFromYUV422PData extends NVImage {
  constructor() {
    super(3, 3, 2, 2, "YUV420SP_NV12");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    // U
    this.uvData[0] = 109;
    this.uvData[2] = 54;
    this.uvData[4] = 147;
    this.uvData[6] = 147;

    // V
    this.uvData[1] = 184;
    this.uvData[3] = 34;
    this.uvData[5] = 152;
    this.uvData[7] = 72;
  };
};

class NV21Data extends NVImage {
  constructor() {
    super(3, 3, 2, 2, "YUV420SP_NV21");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    // U
    this.uvData[1] = 119;
    this.uvData[3] = 110;
    this.uvData[5] = 165;
    this.uvData[7] = 128;

    // V
    this.uvData[0] = 156;
    this.uvData[2] = 25;
    this.uvData[4] = 175;
    this.uvData[6] = 128;
  };
};

class NV21DataFromYUV444PData extends NVImage {
  constructor() {
    super(3, 3, 2, 2, "YUV420SP_NV12");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    // V
    this.uvData[1] = 133;
    this.uvData[3] = 78;
    this.uvData[5] = 181;
    this.uvData[7] = 126;

    // U
    this.uvData[0] = 145;
    this.uvData[2] = 74;
    this.uvData[4] = 176;
    this.uvData[6] = 106;
  };
};

class NV21DataFromYUV422PData extends NVImage {
  constructor() {
    super(3, 3, 2, 2, "YUV420SP_NV12");

    this.yData[0] = 16;  this.yData[1] = 82;  this.yData[2] = 144;
    this.yData[3] = 41;  this.yData[4] = 210; this.yData[5] = 169;
    this.yData[6] = 107; this.yData[7] = 235; this.yData[8] = 126;

    // V
    this.uvData[1] = 109;
    this.uvData[3] = 54;
    this.uvData[5] = 147;
    this.uvData[7] = 147;

    // U
    this.uvData[0] = 184;
    this.uvData[2] = 34;
    this.uvData[4] = 152;
    this.uvData[6] = 72;
  };
};

class HSVData extends FloatSimpleImage {
  constructor() {
    super(3, 3, 3, "HSV");

    var i = 0;
    this.data[i + 0] = 0.0;   this.data[i + 1] = 0.0; this.data[i + 2] = 0.0;
    this.data[i + 3] = 0.0;   this.data[i + 4] = 1.0; this.data[i + 5] = 1.0;
    this.data[i + 6] = 120.0; this.data[i + 7] = 1.0; this.data[i + 8] = 1.0;

    i += this.width * this.channelCount;
    this.data[i + 0] = 240.0; this.data[i + 1] = 1.0; this.data[i + 2] = 1.0;
    this.data[i + 3] = 60.0;  this.data[i + 4] = 1.0; this.data[i + 5] = 1.0;
    this.data[i + 6] = 180.0; this.data[i + 7] = 1.0; this.data[i + 8] = 1.0;

    i += this.width * this.channelCount;
    this.data[i + 0] = 300.0; this.data[i + 1] = 1.0; this.data[i + 2] = 1.0;
    this.data[i + 3] = 0.0;   this.data[i + 4] = 0.0; this.data[i + 5] = 1.0;
    this.data[i + 6] = 0.0;   this.data[i + 7] = 0.0; this.data[i + 8] = 0.50196081;
  };
};

class HSVDataFromLabData extends FloatSimpleImage {
  constructor() {
    super(3, 3, 3, "HSV");

    var i = 0;
    this.data[i + 0] = 0.0;   this.data[i + 1] = 0.0; this.data[i + 2] = 0.0;
    this.data[i + 3] = 0.0;   this.data[i + 4] = 1.0; this.data[i + 5] = 0.996078;
    this.data[i + 6] = 120.0; this.data[i + 7] = 1.0; this.data[i + 8] = 1.0;

    i += this.width * this.channelCount;
    this.data[i + 0] = 240.0;       this.data[i + 1] = 1.0; this.data[i + 2] = 1.0;
    this.data[i + 3] = 60.235294;   this.data[i + 4] = 1.0; this.data[i + 5] = 1.0;
    this.data[i + 6] = 179.764706;  this.data[i + 7] = 1.0; this.data[i + 8] = 1.0;

    i += this.width * this.channelCount;
    this.data[i + 0] = 300.0;       this.data[i + 1] = 1.0;       this.data[i + 2] = 1.0;
    this.data[i + 3] = 0.0;         this.data[i + 4] = 0.003922;  this.data[i + 5] = 1.0;
    this.data[i + 6] = 59.999998;   this.data[i + 7] = 0.007813;  this.data[i + 8] = 0.501961;
  };
};

class HSVDataFromYUV444PData extends FloatSimpleImage {
  constructor() {
    super(3, 3, 3, "HSV");

    var i = 0;
    this.data[i + 0] = 0.0;                 this.data[i + 1] = 0.0;                 this.data[i + 2] = 0.0;
    this.data[i + 3] = 0.0;                 this.data[i + 4] = 1.0000000001003937;  this.data[i + 5] = 0.996078431372549;
    this.data[i + 6] = 120.23715415017372;  this.data[i + 7] = 1.0000000001007905;  this.data[i + 8] = 0.9921568627450981;

    i += this.width * this.channelCount;
    this.data[i + 0] = 240.0;               this.data[i + 1] = 1.0000000001015936;  this.data[i + 2] = 0.984313725490196;
    this.data[i + 3] = 59.99999999390438;   this.data[i + 4] = 0.9920948617608696;  this.data[i + 5] = 0.9921568627450981;
    this.data[i + 6] = 179.76284584377885;  this.data[i + 7] = 1.0000000001007905;  this.data[i + 8] = 0.9921568627450981;

    i += this.width * this.channelCount;
    this.data[i + 0] = 300.7058823588706;   this.data[i + 1] = 1.0000000001;        this.data[i + 2] = 1.0;
    this.data[i + 3] = 0.0;                 this.data[i + 4] = 0.0;                 this.data[i + 5] = 0.9921568627450981;
    this.data[i + 6] = 0.0;                 this.data[i + 7] = 0.0;                 this.data[i + 8] = 0.4980392156862745;
  };
};

class HSVDataFromYUV422PData extends FloatSimpleImage {
  constructor() {
    super(3, 3, 3, "HSV");

    var i = 0;
    this.data[i + 0] = 0.0;                 this.data[i + 1] = 1.0000000002865168; this.data[i + 2] = 0.34901960784313724;
    this.data[i + 3] = 0.0;                 this.data[i + 4] = 0.7696969698515151; this.data[i + 5] = 0.6470588235294118;
    this.data[i + 6] = 120.23715415017372;  this.data[i + 7] = 1.0000000001007905; this.data[i + 8] = 0.9921568627450981;

    i += this.width * this.channelCount;
    this.data[i + 0] = 0;                   this.data[i + 1] = 0.0;                 this.data[i + 2] = 0.10980392156862745;
    this.data[i + 3] = 0;                   this.data[i + 4] = 0.0;                 this.data[i + 5] = 0.8784313725490196;
    this.data[i + 6] = 179.76284584377885;  this.data[i + 7] = 1.0000000001007905;  this.data[i + 8] = 0.9921568627450981;

    i += this.width * this.channelCount;
    this.data[i + 0] = 300.93750001176636;  this.data[i + 1] = 0.7111111112527777;  this.data[i + 2] = 0.7058823529411765;
    this.data[i + 3] = 300.0000000278182;   this.data[i + 4] = 0.21568627460980394; this.data[i + 5] = 1.0;
    this.data[i + 6] = 0.0;                 this.data[i + 7] = 0.0;                 this.data[i + 8] = 0.4980392156862745;
  };
};

class HSVDataFromYUV420PData extends FloatSimpleImage {
  constructor() {
    super(3, 3, 3, "HSV");

    var i = 0;
    this.data[i + 0] = 0.0;                 this.data[i + 1] = 1.0000000005795455;  this.data[i + 2] = 0.17254901960784313;
    this.data[i + 3] = 359.04761904800455;  this.data[i + 4] = 0.5250000002125;     this.data[i + 5] = 0.47058823529411764;
    this.data[i + 6] = 148.23529411462184;  this.data[i + 7] = 1.000000000107143;   this.data[i + 8] = 0.9333333333333333;

    i += this.width * this.channelCount;
    this.data[i + 0] = 358.1250000007471;   this.data[i + 1] = 0.8767123291164385;  this.data[i + 2] = 0.28627450980392155;
    this.data[i + 3] = 358.800000000612;    this.data[i + 4] = 0.196078431472549;   this.data[i + 5] = 1.0;
    this.data[i + 6] = 151.85185184850937;  this.data[i + 7] = 0.9529411765705882;  this.data[i + 8] = 1.0;

    i += this.width * this.channelCount;
    this.data[i + 0] = 300.93750001176636;  this.data[i + 1] = 0.7111111112527777;  this.data[i + 2] = 0.7058823529411765;
    this.data[i + 3] = 300.0000000278182;   this.data[i + 4] = 0.21568627460980394; this.data[i + 5] = 1.0;
    this.data[i + 6] = 0.0;                 this.data[i + 7] = 0.0;                 this.data[i + 8] = 0.4980392156862745;
  };
};

class LabData extends FloatSimpleImage {
  constructor() {
    super(3, 3, 3, "Lab");

    var i = 0;
    this.data[i + 0] = 0.0;       this.data[i + 1] = 0.0;         this.data[i + 2] = 0.0;
    this.data[i + 3] = 53.240585; this.data[i + 4] = 80.094185;   this.data[i + 5] = 67.201538;
    this.data[i + 6] = 87.7351;   this.data[i + 7] = -86.181252;  this.data[i + 8] = 83.177483;

    i += this.width * this.channelCount;
    this.data[i + 0] = 32.29567;  this.data[i + 1] = 79.186989;   this.data[i + 2] = -107.86176;
    this.data[i + 3] = 97.139511; this.data[i + 4] = -21.552414;  this.data[i + 5] = 94.475792;
    this.data[i + 6] = 91.113297; this.data[i + 7] = -48.088551;  this.data[i + 8] = -14.130962;

    i += this.width * this.channelCount;
    this.data[i + 0] = 60.323502; this.data[i + 1] = 98.235161;   this.data[i + 2] = -60.825493;
    this.data[i + 3] = 100.0;     this.data[i + 4] = 0.0;         this.data[i + 5] = 0.0;
    this.data[i + 6] = 53.585014; this.data[i + 7] = 0.0;         this.data[i + 8] = 0.0;
  };
};

class LabDataFromYUV444PData extends FloatSimpleImage {
  constructor() {
    super(3, 3, 3, "HSV");

    var i = 0;
    this.data[i + 0] = 0.0;                 this.data[i + 1] = 0.0;                 this.data[i + 2] = 0.0;
    this.data[i + 3] = 53.034610465388056;  this.data[i + 4] = 79.85590203914461;   this.data[i + 5] = 67.0016253024788;
    this.data[i + 6] = 87.11875689267448;   this.data[i + 7] = -85.65429374039535;  this.data[i + 8] = 82.60623202041464;

    i += this.width * this.channelCount;
    this.data[i + 0] = 31.720345672804157;  this.data[i + 1] = 78.24367895044873;   this.data[i + 2] = -106.5768337072531;
    this.data[i + 3] = 96.46792120648958;   this.data[i + 4] = -21.409519847697347; this.data[i + 5] = 93.77548135780542;
    this.data[i + 6] = 90.44660871821826;   this.data[i + 7] = -48.089026724461526; this.data[i + 8] = -13.571034820412686;

    i += this.width * this.channelCount;
    this.data[i + 0] = 60.151950936932494;  this.data[i + 1] = 97.82071254270292;   this.data[i + 2] = -59.43734830934828;
    this.data[i + 3] = 99.30958687208283;   this.data[i + 4] = 0.0;                 this.data[i + 5] = 0.0;
    this.data[i + 6] = 53.19277745493915;   this.data[i + 7] = 0.0;                 this.data[i + 8] = 0.0;
  };
};

class LabDataFromYUV422PData extends FloatSimpleImage {
  constructor() {
    super(3, 3, 3, "HSV");

    var i = 0;
    this.data[i + 0] = 16.127781199491146; this.data[i + 1] = 37.16386506574049;  this.data[i + 2] = 25.043689417354877;
    this.data[i + 3] = 36.981683077525915; this.data[i + 4] = 50.903511613481115; this.data[i + 5] = 32.31142038484883;
    this.data[i + 6] = 87.11875689267448;  this.data[i + 7] = -85.65429374039535; this.data[i + 8] = 82.60623202041464;

    i += this.width * this.channelCount;
    this.data[i + 0] = 10.268184311230112; this.data[i + 1] = 0.0;                 this.data[i + 2] = 0.0;
    this.data[i + 3] = 89.1772802290269;   this.data[i + 4] = 0.0;                 this.data[i + 5] = 0.0;
    this.data[i + 6] = 90.44660871821826;  this.data[i + 7] = -48.089026724461526; this.data[i + 8] = -13.571034820412686;

    i += this.width * this.channelCount;
    this.data[i + 0] = 46.144074613608296; this.data[i + 1] = 65.16894552944552;  this.data[i + 2] = -40.267933584999625;
    this.data[i + 3] = 86.8938834807636;   this.data[i + 4] = 28.462923575986455; this.data[i + 5] = -19.464966592414633;
    this.data[i + 6] = 53.19277745493915;  this.data[i + 7] = 0.0;                this.data[i + 8] = 0.0;
  };
};

class LabDataFromYUV420PData extends FloatSimpleImage {
  constructor() {
    super(3, 3, 3, "HSV");

    var i = 0;
    this.data[i + 0] = 4.838519820745088;  this.data[i + 1] = 21.141105340568455; this.data[i + 2] = 7.645700032099379;
    this.data[i + 3] = 32.31563239702677;  this.data[i + 4] = 27.57546933915833;  this.data[i + 5] = 12.300896526188554;
    this.data[i + 6] = 83.08065258991849;  this.data[i + 7] = -73.895752859582;   this.data[i + 8] = 47.405921341516176;

    i += this.width * this.channelCount;
    this.data[i + 0] = 13.450503010545155; this.data[i + 1] = 29.406984528513203;  this.data[i + 2] = 16.333350166067607;
    this.data[i + 3] = 86.69267491397105;  this.data[i + 4] = 17.77388194061319;   this.data[i + 5] = 6.21670853560361;
    this.data[i + 6] = 88.693447032887;    this.data[i + 7] = -74.34828426368617;  this.data[i + 8] = 40.64106565615555;

    i += this.width * this.channelCount;
    this.data[i + 0] = 46.144074613608296; this.data[i + 1] = 65.16894552944552;  this.data[i + 2] = -40.267933584999625;
    this.data[i + 3] = 86.8938834807636;   this.data[i + 4] = 28.462923575986455; this.data[i + 5] = -19.464966592414633;
    this.data[i + 6] = 53.19277745493915;  this.data[i + 7] = 0.0;                this.data[i + 8] = 0.0;
  };
};

class DepthData extends Uint16SimpleImage {
  constructor() {
    super(3, 3, 1, "DEPTH");

    var i = 0;
    this.data[i + 0] = 2;   this.data[i + 1] = 4;   this.data[i + 2] = 8;

    i += this.width * this.channelCount;
    this.data[i + 0] = 16;  this.data[i + 1] = 32;  this.data[i + 2] = 64;

    i += this.width * this.channelCount;
    this.data[i + 0] = 128; this.data[i + 1] = 256; this.data[i + 2] = 512;
  };
};

function getData(format, originalFormat) {
  if (format == "RGBA32") {
    if (originalFormat == "YUV444P") {
      return new RGBA32DataFromYUV444PData(0, 1, 2, 3);
    } else if (originalFormat == "YUV422P") {
      return new RGBA32DataFromYUV422PData(0, 1, 2, 3);
    } else if (originalFormat == "YUV420P" ||
               originalFormat == "YUV420SP_NV12" ||
               originalFormat == "YUV420SP_NV21") {
      return new RGBA32DataFromYUV420PData(0, 1, 2, 3);
    } else {
      return new RGBA32Data();
    }
  } else if (format == "BGRA32") {
    if (originalFormat == "YUV444P") {
      return new RGBA32DataFromYUV444PData(2, 1, 0, 3);
    } else if (originalFormat == "YUV422P") {
      return new RGBA32DataFromYUV422PData(2, 1, 0, 3);
    } else if (originalFormat == "YUV420P" ||
               originalFormat == "YUV420SP_NV12" ||
               originalFormat == "YUV420SP_NV21") {
      return new RGBA32DataFromYUV420PData(2, 1, 0, 3);
    } else {
      return new BGRA32Data();
    }
  } else if (format == "RGB24") {
    if (originalFormat == "YUV444P") {
      return new RGBA32DataFromYUV444PData(0, 1, 2);
    } else if (originalFormat == "YUV422P") {
      return new RGBA32DataFromYUV422PData(0, 1, 2);
    } else if (originalFormat == "YUV420P" ||
               originalFormat == "YUV420SP_NV12" ||
               originalFormat == "YUV420SP_NV21") {
      return new RGBA32DataFromYUV420PData(0, 1, 2);
    } else {
      return new RGB24Data();
    }
  } else if (format == "BGR24") {
    if (originalFormat == "YUV444P") {
      return new RGBA32DataFromYUV444PData(2, 1, 0);
    } else if (originalFormat == "YUV422P") {
      return new RGBA32DataFromYUV422PData(2, 1, 0);
    } else if (originalFormat == "YUV420P" ||
               originalFormat == "YUV420SP_NV12" ||
               originalFormat == "YUV420SP_NV21") {
      return new RGBA32DataFromYUV420PData(2, 1, 0);
    } else {
      return new BGR24Data();
    }
  } else if (format == "GRAY8") {
    if (originalFormat == "YUV444P" ||
        originalFormat == "YUV422P" ||
        originalFormat == "YUV420P" ||
        originalFormat == "YUV420SP_NV12" ||
        originalFormat == "YUV420SP_NV21") {
      return  new Gray8DataFromYUVData();
    } else {
      return new Gray8Data();
    }
  } else if (format == "YUV444P") {
    if (originalFormat == "YUV422P") {
      return new YUV444PDataFromYUV422PData();
    } else if (originalFormat == "YUV420P" ||
               originalFormat == "YUV420SP_NV12" ||
               originalFormat == "YUV420SP_NV21") {
      return new YUV444PDataFromYUV420PData();
    } else {
      return new YUV444PData();
    }
  } else if (format == "YUV422P") {
    if (originalFormat == "YUV444P") {
      return new YUV422PDataFromYUV444PData();
    } else if (originalFormat == "YUV420P" ||
               originalFormat == "YUV420SP_NV12" ||
               originalFormat == "YUV420SP_NV21") {
      return new YUV422PDataFromYUV420PData();
    } else {
      return new YUV422PData();
    }
  } else if (format == "YUV420P") {
    if (originalFormat == "YUV444P") {
      return new YUV420PDataFromYUV444PData();
    } else if (originalFormat == "YUV422P") {
      return new YUV420PDataFromYUV422PData();
    } else {
      return new YUV420PData();
    }
  } else if (format == "YUV420SP_NV12") {
    if (originalFormat == "YUV444P") {
      return new NV12DataFromYUV444PData();
    } else if (originalFormat == "YUV422P") {
      return new NV12DataFromYUV422PData();
    } else {
      return new NV12Data();
    }
  } else if (format == "YUV420SP_NV21") {
    if (originalFormat == "YUV444P") {
      return new NV21DataFromYUV444PData();
    } else if (originalFormat == "YUV422P") {
      return new NV21DataFromYUV422PData();
    } else {
      return new NV21Data();
    }
  } else if (format == "HSV") {
    if (originalFormat == "YUV444P") {
      return new HSVDataFromYUV444PData();
    } else if (originalFormat == "YUV422P") {
      return new HSVDataFromYUV422PData();
    } else if (originalFormat == "YUV420P" ||
               originalFormat == "YUV420SP_NV12" ||
               originalFormat == "YUV420SP_NV21") {
      return new HSVDataFromYUV420PData();
    } else if (originalFormat == "Lab") {
      return new HSVDataFromLabData();
    } else {
      return new HSVData();
    }
  } else if (format == "Lab") {
    if (originalFormat == "YUV444P") {
      return new LabDataFromYUV444PData();
    } else if (originalFormat == "YUV422P") {
      return new LabDataFromYUV422PData();
    } else if (originalFormat == "YUV420P" ||
               originalFormat == "YUV420SP_NV12" ||
               originalFormat == "YUV420SP_NV21") {
      return new LabDataFromYUV420PData();
    } else {
      return new LabData();
    }
  } else if (format == "DEPTH") {
    return new DepthData();
  }
}

function getTestData(sourceFromat, destinationFormat) {
  return [getData(sourceFromat), getData(destinationFormat, sourceFromat)];
}