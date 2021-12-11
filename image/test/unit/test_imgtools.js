/*
 * Tests for imgITools
 */

const { NetUtil } = ChromeUtils.import("resource://gre/modules/NetUtil.jsm");

/*
 * dumpToFile()
 *
 * For test development, dumps the specified array to a file.
 * Call |dumpToFile(outData);| in a test to file to a file.
 */
function dumpToFile(aData) {
  var outputFile = do_get_tempdir();
  outputFile.append("testdump.png");

  var outputStream = Cc[
    "@mozilla.org/network/file-output-stream;1"
  ].createInstance(Ci.nsIFileOutputStream);
  // WR_ONLY|CREATE|TRUNC
  outputStream.init(outputFile, 0x02 | 0x08 | 0x20, 0o644, null);

  var bos = Cc["@mozilla.org/binaryoutputstream;1"].createInstance(
    Ci.nsIBinaryOutputStream
  );
  bos.setOutputStream(outputStream);

  bos.writeByteArray(aData);

  outputStream.close();
}

/*
 * getFileInputStream()
 *
 * Returns an input stream for the specified file.
 */
function getFileInputStream(aFile) {
  var inputStream = Cc[
    "@mozilla.org/network/file-input-stream;1"
  ].createInstance(Ci.nsIFileInputStream);
  // init the stream as RD_ONLY, -1 == default permissions.
  inputStream.init(aFile, 0x01, -1, null);

  // Blah. The image decoders use ReadSegments, which isn't implemented on
  // file input streams. Use a buffered stream to make it work.
  var bis = Cc["@mozilla.org/network/buffered-input-stream;1"].createInstance(
    Ci.nsIBufferedInputStream
  );
  bis.init(inputStream, 1024);

  return bis;
}

/*
 * streamToArray()
 *
 * Consumes an input stream, and returns its bytes as an array.
 */
function streamToArray(aStream) {
  var size = aStream.available();

  // use a binary input stream to grab the bytes.
  var bis = Cc["@mozilla.org/binaryinputstream;1"].createInstance(
    Ci.nsIBinaryInputStream
  );
  bis.setInputStream(aStream);

  var bytes = bis.readByteArray(size);
  if (size != bytes.length) {
    throw new Error("Didn't read expected number of bytes");
  }

  return bytes;
}

/*
 * compareArrays
 *
 * Compares two arrays, and throws if there's a difference.
 */
function compareArrays(aArray1, aArray2) {
  Assert.equal(aArray1.length, aArray2.length);

  for (var i = 0; i < aArray1.length; i++) {
    if (aArray1[i] != aArray2[i]) {
      throw new Error("arrays differ at index " + i);
    }
  }
}

/*
 * checkExpectedError
 *
 * Checks to see if a thrown error was expected or not, and if it
 * matches the expected value.
 */
function checkExpectedError(aExpectedError, aActualError) {
  if (aExpectedError) {
    if (!aActualError) {
      throw new Error("Didn't throw as expected (" + aExpectedError + ")");
    }

    if (!aExpectedError.test(aActualError)) {
      throw new Error("Threw (" + aActualError + "), not (" + aExpectedError);
    }

    // We got the expected error, so make a note in the test log.
    dump("...that error was expected.\n\n");
  } else if (aActualError) {
    throw new Error("Threw unexpected error: " + aActualError);
  }
}

function run_test() {
  try {
    /* ========== 0 ========== */
    var testnum = 0;
    var testdesc = "imgITools setup";
    var err = null;

    var imgTools = Cc["@mozilla.org/image/tools;1"].getService(Ci.imgITools);

    if (!imgTools) {
      throw new Error("Couldn't get imgITools service");
    }

    // Ugh, this is an ugly hack. The pixel values we get in Windows are sometimes
    // +/- 1 value compared to other platforms, so we need to compare against a
    // different set of reference images. nsIXULRuntime.OS doesn't seem to be
    // available in xpcshell, so we'll use this as a kludgy way to figure out if
    // we're running on Windows.
    var isWindows = mozinfo.os == "win";

    /* ========== 1 ========== */
    testnum++;
    testdesc = "test decoding a PNG";

    // 64x64 png, 8415 bytes.
    var imgName = "image1.png";
    var inMimeType = "image/png";
    var imgFile = do_get_file(imgName);

    var istream = getFileInputStream(imgFile);
    Assert.equal(istream.available(), 8415);

    var buffer = NetUtil.readInputStreamToString(istream, istream.available());
    var container = imgTools.decodeImageFromBuffer(
      buffer,
      buffer.length,
      inMimeType
    );

    // It's not easy to look at the pixel values from JS, so just
    // check the container's size.
    Assert.equal(container.width, 64);
    Assert.equal(container.height, 64);

    /* ========== 2 ========== */
    testnum++;
    testdesc = "test encoding a scaled JPEG";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeScaledImage(container, "image/jpeg", 16, 16);

    var encodedBytes = streamToArray(istream);
    // Get bytes for expected result
    var refName = "image1png16x16.jpg";
    var refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 1050);
    var referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 3 ========== */
    testnum++;
    testdesc = "test encoding an unscaled JPEG";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeImage(container, "image/jpeg");
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image1png64x64.jpg";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 4507);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 4 ========== */
    testnum++;
    testdesc = "test decoding a JPEG";

    // 32x32 jpeg, 3494 bytes.
    imgName = "image2.jpg";
    inMimeType = "image/jpeg";
    imgFile = do_get_file(imgName);

    istream = getFileInputStream(imgFile);
    Assert.equal(istream.available(), 3494);

    buffer = NetUtil.readInputStreamToString(istream, istream.available());
    container = imgTools.decodeImageFromBuffer(
      buffer,
      buffer.length,
      inMimeType
    );

    // It's not easy to look at the pixel values from JS, so just
    // check the container's size.
    Assert.equal(container.width, 32);
    Assert.equal(container.height, 32);

    /* ========== 5 ========== */
    testnum++;
    testdesc = "test encoding a scaled PNG";

    if (!isWindows) {
      // we'll reuse the container from the previous test
      istream = imgTools.encodeScaledImage(container, "image/png", 16, 16);

      encodedBytes = streamToArray(istream);
      // Get bytes for expected result
      refName = isWindows ? "image2jpg16x16-win.png" : "image2jpg16x16.png";
      refFile = do_get_file(refName);
      istream = getFileInputStream(refFile);
      Assert.equal(istream.available(), 950);
      referenceBytes = streamToArray(istream);

      // compare the encoder's output to the reference file.
      compareArrays(encodedBytes, referenceBytes);
    }

    /* ========== 6 ========== */
    testnum++;
    testdesc = "test encoding an unscaled PNG";

    if (!isWindows) {
      // we'll reuse the container from the previous test
      istream = imgTools.encodeImage(container, "image/png");
      encodedBytes = streamToArray(istream);

      // Get bytes for expected result
      refName = isWindows ? "image2jpg32x32-win.png" : "image2jpg32x32.png";
      refFile = do_get_file(refName);
      istream = getFileInputStream(refFile);
      Assert.equal(istream.available(), 3105);
      referenceBytes = streamToArray(istream);

      // compare the encoder's output to the reference file.
      compareArrays(encodedBytes, referenceBytes);
    }

    /* ========== 7 ========== */
    testnum++;
    testdesc = "test decoding a ICO";

    // 16x16 ico, 1406 bytes.
    imgName = "image3.ico";
    inMimeType = "image/x-icon";
    imgFile = do_get_file(imgName);

    istream = getFileInputStream(imgFile);
    Assert.equal(istream.available(), 1406);

    buffer = NetUtil.readInputStreamToString(istream, istream.available());
    container = imgTools.decodeImageFromBuffer(
      buffer,
      buffer.length,
      inMimeType
    );

    // It's not easy to look at the pixel values from JS, so just
    // check the container's size.
    Assert.equal(container.width, 16);
    Assert.equal(container.height, 16);

    /* ========== 8 ========== */
    testnum++;
    testdesc = "test encoding a scaled PNG"; // note that we're scaling UP

    // we'll reuse the container from the previous test
    istream = imgTools.encodeScaledImage(container, "image/png", 32, 32);
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image3ico32x32.png";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 2285);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 9 ========== */
    testnum++;
    testdesc = "test encoding an unscaled PNG";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeImage(container, "image/png");
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image3ico16x16.png";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 330);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 10 ========== */
    testnum++;
    testdesc = "test decoding a GIF";

    // 32x32 gif, 1809 bytes.
    imgName = "image4.gif";
    inMimeType = "image/gif";
    imgFile = do_get_file(imgName);

    istream = getFileInputStream(imgFile);
    Assert.equal(istream.available(), 1809);

    buffer = NetUtil.readInputStreamToString(istream, istream.available());
    container = imgTools.decodeImageFromBuffer(
      buffer,
      buffer.length,
      inMimeType
    );

    // It's not easy to look at the pixel values from JS, so just
    // check the container's size.
    Assert.equal(container.width, 32);
    Assert.equal(container.height, 32);

    /* ========== 11 ========== */
    testnum++;
    testdesc =
      "test encoding an unscaled ICO with format options " +
      "(format=bmp;bpp=32)";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeImage(
      container,
      "image/vnd.microsoft.icon",
      "format=bmp;bpp=32"
    );
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image4gif32x32bmp32bpp.ico";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 4286);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 12 ========== */
    testnum++;
    testdesc =
      // eslint-disable-next-line no-useless-concat
      "test encoding a scaled ICO with format options " + "(format=bmp;bpp=32)";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeScaledImage(
      container,
      "image/vnd.microsoft.icon",
      16,
      16,
      "format=bmp;bpp=32"
    );
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image4gif16x16bmp32bpp.ico";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 1150);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 13 ========== */
    testnum++;
    testdesc =
      "test encoding an unscaled ICO with format options " +
      "(format=bmp;bpp=24)";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeImage(
      container,
      "image/vnd.microsoft.icon",
      "format=bmp;bpp=24"
    );
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image4gif32x32bmp24bpp.ico";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 3262);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 14 ========== */
    testnum++;
    testdesc =
      // eslint-disable-next-line no-useless-concat
      "test encoding a scaled ICO with format options " + "(format=bmp;bpp=24)";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeScaledImage(
      container,
      "image/vnd.microsoft.icon",
      16,
      16,
      "format=bmp;bpp=24"
    );
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image4gif16x16bmp24bpp.ico";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 894);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 15 ========== */
    testnum++;
    testdesc = "test cropping a JPG";

    // 32x32 jpeg, 3494 bytes.
    imgName = "image2.jpg";
    inMimeType = "image/jpeg";
    imgFile = do_get_file(imgName);

    istream = getFileInputStream(imgFile);
    Assert.equal(istream.available(), 3494);

    buffer = NetUtil.readInputStreamToString(istream, istream.available());
    container = imgTools.decodeImageFromBuffer(
      buffer,
      buffer.length,
      inMimeType
    );

    // It's not easy to look at the pixel values from JS, so just
    // check the container's size.
    Assert.equal(container.width, 32);
    Assert.equal(container.height, 32);

    // encode a cropped image
    istream = imgTools.encodeCroppedImage(
      container,
      "image/jpeg",
      0,
      0,
      16,
      16
    );
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image2jpg16x16cropped.jpg";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 879);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 16 ========== */
    testnum++;
    testdesc = "test cropping a JPG with an offset";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeCroppedImage(
      container,
      "image/jpeg",
      16,
      16,
      16,
      16
    );
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image2jpg16x16cropped2.jpg";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 878);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 17 ========== */
    testnum++;
    testdesc = "test cropping a JPG without a given height";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeCroppedImage(container, "image/jpeg", 0, 0, 16, 0);
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image2jpg16x32cropped3.jpg";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 1127);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 18 ========== */
    testnum++;
    testdesc = "test cropping a JPG without a given width";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeCroppedImage(container, "image/jpeg", 0, 0, 0, 16);
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image2jpg32x16cropped4.jpg";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 1135);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 19 ========== */
    testnum++;
    testdesc = "test cropping a JPG without a given width and height";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeCroppedImage(container, "image/jpeg", 0, 0, 0, 0);
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image2jpg32x32.jpg";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 1634);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 20 ========== */
    testnum++;
    testdesc = "test scaling a JPG without a given width";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeScaledImage(container, "image/jpeg", 0, 16);
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image2jpg32x16scaled.jpg";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 1227);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 21 ========== */
    testnum++;
    testdesc = "test scaling a JPG without a given height";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeScaledImage(container, "image/jpeg", 16, 0);
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image2jpg16x32scaled.jpg";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 1219);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 22 ========== */
    testnum++;
    testdesc = "test scaling a JPG without a given width and height";

    // we'll reuse the container from the previous test
    istream = imgTools.encodeScaledImage(container, "image/jpeg", 0, 0);
    encodedBytes = streamToArray(istream);

    // Get bytes for expected result
    refName = "image2jpg32x32.jpg";
    refFile = do_get_file(refName);
    istream = getFileInputStream(refFile);
    Assert.equal(istream.available(), 1634);
    referenceBytes = streamToArray(istream);

    // compare the encoder's output to the reference file.
    compareArrays(encodedBytes, referenceBytes);

    /* ========== 22 ========== */
    testnum++;
    testdesc = "test invalid arguments for cropping";

    var numErrors = 0;

    try {
      // width/height can't be negative
      imgTools.encodeScaledImage(container, "image/jpeg", -1, -1);
    } catch (e) {
      numErrors++;
    }

    try {
      // offsets can't be negative
      imgTools.encodeCroppedImage(container, "image/jpeg", -1, -1, 16, 16);
    } catch (e) {
      numErrors++;
    }

    try {
      // width/height can't be negative
      imgTools.encodeCroppedImage(container, "image/jpeg", 0, 0, -1, -1);
    } catch (e) {
      numErrors++;
    }

    try {
      // out of bounds
      imgTools.encodeCroppedImage(container, "image/jpeg", 17, 17, 16, 16);
    } catch (e) {
      numErrors++;
    }

    try {
      // out of bounds
      imgTools.encodeCroppedImage(container, "image/jpeg", 0, 0, 33, 33);
    } catch (e) {
      numErrors++;
    }

    try {
      // out of bounds
      imgTools.encodeCroppedImage(container, "image/jpeg", 1, 1, 0, 0);
    } catch (e) {
      numErrors++;
    }

    Assert.equal(numErrors, 6);

    /* ========== bug 363986 ========== */
    testnum = 363986;
    testdesc = "test PNG and JPEG encoders' Read/ReadSegments methods";

    var testData = [
      {
        preImage: "image3.ico",
        preImageMimeType: "image/x-icon",
        refImage: "image3ico16x16.png",
        refImageMimeType: "image/png",
      },
      {
        preImage: "image1.png",
        preImageMimeType: "image/png",
        refImage: "image1png64x64.jpg",
        refImageMimeType: "image/jpeg",
      },
    ];

    for (var i = 0; i < testData.length; ++i) {
      var dict = testData[i];

      imgFile = do_get_file(dict.refImage);
      istream = getFileInputStream(imgFile);
      var refBytes = streamToArray(istream);

      imgFile = do_get_file(dict.preImage);
      istream = getFileInputStream(imgFile);

      buffer = NetUtil.readInputStreamToString(istream, istream.available());
      container = imgTools.decodeImageFromBuffer(
        buffer,
        buffer.length,
        dict.preImageMimeType
      );

      istream = imgTools.encodeImage(container, dict.refImageMimeType);

      var sstream = Cc["@mozilla.org/storagestream;1"].createInstance(
        Ci.nsIStorageStream
      );
      sstream.init(4096, 4294967295, null);
      var ostream = sstream.getOutputStream(0);
      var bostream = Cc[
        "@mozilla.org/network/buffered-output-stream;1"
      ].createInstance(Ci.nsIBufferedOutputStream);

      // use a tiny buffer to make sure the image data doesn't fully fit in it
      bostream.init(ostream, 8);

      bostream.writeFrom(istream, istream.available());
      bostream.flush();
      bostream.close();

      var encBytes = streamToArray(sstream.newInputStream(0));

      compareArrays(refBytes, encBytes);
    }

    /* ========== bug 413512  ========== */
    testnum = 413512;
    testdesc = "test decoding bad favicon (bug 413512)";

    imgName = "bug413512.ico";
    inMimeType = "image/x-icon";
    imgFile = do_get_file(imgName);

    istream = getFileInputStream(imgFile);
    Assert.equal(istream.available(), 17759);
    var errsrc = "none";

    try {
      buffer = NetUtil.readInputStreamToString(istream, istream.available());
      container = imgTools.decodeImageFromBuffer(
        buffer,
        buffer.length,
        inMimeType
      );

      // We expect to hit an error during encoding because the ICO header of the
      // image is fine, but the actual resources are corrupt. Since
      // decodeImageFromBuffer() only performs a metadata decode, it doesn't decode
      // far enough to realize this, but we'll find out when we do a full decode
      // during encodeImage().
      try {
        istream = imgTools.encodeImage(container, "image/png");
      } catch (e) {
        err = e;
        errsrc = "encode";
      }
    } catch (e) {
      err = e;
      errsrc = "decode";
    }

    Assert.equal(errsrc, "encode");
    checkExpectedError(/NS_ERROR_FAILURE/, err);

    /* ========== bug 815359  ========== */
    testnum = 815359;
    testdesc = "test correct ico hotspots (bug 815359)";

    imgName = "bug815359.ico";
    inMimeType = "image/x-icon";
    imgFile = do_get_file(imgName);

    istream = getFileInputStream(imgFile);
    Assert.equal(istream.available(), 4286);

    buffer = NetUtil.readInputStreamToString(istream, istream.available());
    container = imgTools.decodeImageFromBuffer(
      buffer,
      buffer.length,
      inMimeType
    );

    Assert.equal(container.hotspotX, 10);
    Assert.equal(container.hotspotY, 9);

    /* ========== end ========== */
  } catch (e) {
    throw new Error(
      "FAILED in test #" + testnum + " -- " + testdesc + ": " + e
    );
  }
}
