/*
 * Tests for imgITools
 */

const Ci = Components.interfaces;
const Cc = Components.classes;


/*
 * dumpToFile()
 *
 * For test development, dumps the specified array to a file.
 * Call |dumpToFile(outData);| in a test to file to a file.
 */
function dumpToFile(aData) {
    const path = "/tmp";

    var outputFile = Cc["@mozilla.org/file/local;1"].
                     createInstance(Ci.nsILocalFile);
    outputFile.initWithPath(path);
    outputFile.append("testdump.png");

    var outputStream = Cc["@mozilla.org/network/file-output-stream;1"].
                       createInstance(Ci.nsIFileOutputStream);
    // WR_ONLY|CREAT|TRUNC
    outputStream.init(outputFile, 0x02 | 0x08 | 0x20, 0644, null);

    var bos = Cc["@mozilla.org/binaryoutputstream;1"].
              createInstance(Ci.nsIBinaryOutputStream);
    bos.setOutputStream(outputStream);

    bos.writeByteArray(aData, aData.length);

    outputStream.close();
}


/*
 * getFileInputStream()
 *
 * Returns an input stream for the specified file.
 */
function getFileInputStream(aFile) {
    var inputStream = Cc["@mozilla.org/network/file-input-stream;1"].
                      createInstance(Ci.nsIFileInputStream);
    // init the stream as RD_ONLY, -1 == default permissions.
    inputStream.init(aFile, 0x01, -1, null);

    // Blah. The image decoders use ReadSegments, which isn't implemented on
    // file input streams. Use a buffered stream to make it work.
    var bis = Cc["@mozilla.org/network/buffered-input-stream;1"].
              createInstance(Ci.nsIBufferedInputStream);
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
    var bis = Cc["@mozilla.org/binaryinputstream;1"].
              createInstance(Ci.nsIBinaryInputStream);
    bis.setInputStream(aStream);

    var bytes = bis.readByteArray(size);
    if (size != bytes.length)
        throw "Didn't read expected number of bytes";

    return bytes;
}


/*
 * compareArrays
 *
 * Compares two arrays, and throws if there's a difference.
 */
function compareArrays(aArray1, aArray2) {
    do_check_eq(aArray1.length, aArray2.length);

    for (var i = 0; i < aArray1.length; i++)
        if (aArray1[i] != aArray2[i])
            throw "arrays differ at index " + i;
}


/*
 * checkExpectedError
 *
 * Checks to see if a thrown error was expected or not, and if it
 * matches the expected value.
 */
function checkExpectedError (aExpectedError, aActualError) {
  if (aExpectedError) {
      if (!aActualError)
          throw "Didn't throw as expected (" + aExpectedError + ")";

      if (!aExpectedError.test(aActualError))
          throw "Threw (" + aActualError + "), not (" + aExpectedError;

      // We got the expected error, so make a note in the test log.
      dump("...that error was expected.\n\n");
  } else if (aActualError) {
      throw "Threw unexpected error: " + aActualError;
  }
}


function run_test() {

try {


/* ========== 0 ========== */
var testnum = 0;
var testdesc = "imgITools setup";
var err = null;

var imgTools = Cc["@mozilla.org/image/tools;1"].
               getService(Ci.imgITools);

if (!imgTools)
    throw "Couldn't get imgITools service"

// Ugh, this is an ugly hack. The pixel values we get in Windows are sometimes
// +/- 1 value compared to other platforms, so we need to compare against a
// different set of reference images. nsIXULRuntime.OS doesn't seem to be
// available in xpcshell, so we'll use this as a kludgy way to figure out if
// we're running on Windows.
var isWindows = ("@mozilla.org/windows-registry-key;1" in Cc);


/* ========== 1 ========== */
testnum++;
testdesc = "test decoding a PNG";

// 64x64 png, 10698 bytes.
var imgName = "image1.png";
var inMimeType = "image/png";
var imgFile = do_get_file(imgName);

var istream = getFileInputStream(imgFile);
do_check_eq(istream.available(), 10698);

var outParam = { value: null };
imgTools.decodeImageData(istream, inMimeType, outParam);
var container = outParam.value;

// It's not easy to look at the pixel values from JS, so just
// check the container's size.
do_check_eq(container.width,  64);
do_check_eq(container.height, 64);


/* ========== 2 ========== */
testnum++;
testdesc = "test encoding a scaled JPEG";

// we'll reuse the container from the previous test
istream = imgTools.encodeScaledImage(container, "image/jpeg", 16, 16);

var encodedBytes = streamToArray(istream);
// Get bytes for exected result
var refName = "image1png16x16.jpg";
var refFile = do_get_file(refName);
istream = getFileInputStream(refFile);
do_check_eq(istream.available(), 1078);
var referenceBytes = streamToArray(istream);

// compare the encoder's output to the reference file.
compareArrays(encodedBytes, referenceBytes);


/* ========== 3 ========== */
testnum++;
testdesc = "test encoding an unscaled JPEG";

// we'll reuse the container from the previous test
istream = imgTools.encodeImage(container, "image/jpeg");
encodedBytes = streamToArray(istream);

// Get bytes for exected result
refName = "image1png64x64.jpg";
refFile = do_get_file(refName);
istream = getFileInputStream(refFile);
do_check_eq(istream.available(), 4503);
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
do_check_eq(istream.available(), 3494);

outParam = {};
imgTools.decodeImageData(istream, inMimeType, outParam);
container = outParam.value;

// It's not easy to look at the pixel values from JS, so just
// check the container's size.
do_check_eq(container.width,  32);
do_check_eq(container.height, 32);


/* ========== 5 ========== */
testnum++;
testdesc = "test encoding a scaled PNG";

if (!isWindows) {
// we'll reuse the container from the previous test
istream = imgTools.encodeScaledImage(container, "image/png", 16, 16);

encodedBytes = streamToArray(istream);
// Get bytes for exected result
refName = isWindows ? "image2jpg16x16-win.png" : "image2jpg16x16.png";
refFile = do_get_file(refName);
istream = getFileInputStream(refFile);
do_check_eq(istream.available(), 948);
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

// Get bytes for exected result
refName = isWindows ? "image2jpg32x32-win.png" : "image2jpg32x32.png";
refFile = do_get_file(refName);
istream = getFileInputStream(refFile);
do_check_eq(istream.available(), 3105);
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
do_check_eq(istream.available(), 1406);

outParam = { value: null };
imgTools.decodeImageData(istream, inMimeType, outParam);
container = outParam.value;

// It's not easy to look at the pixel values from JS, so just
// check the container's size.
do_check_eq(container.width,  16);
do_check_eq(container.height, 16);


/* ========== 8 ========== */
testnum++;
testdesc = "test encoding a scaled PNG"; // note that we're scaling UP

// we'll reuse the container from the previous test
istream = imgTools.encodeScaledImage(container, "image/png", 32, 32);
encodedBytes = streamToArray(istream);

// Get bytes for exected result
refName = "image3ico32x32.png";
refFile = do_get_file(refName);
istream = getFileInputStream(refFile);
do_check_eq(istream.available(), 2281);
referenceBytes = streamToArray(istream);

// compare the encoder's output to the reference file.
compareArrays(encodedBytes, referenceBytes);


/* ========== 9 ========== */
testnum++;
testdesc = "test encoding an unscaled PNG";

// we'll reuse the container from the previous test
istream = imgTools.encodeImage(container, "image/png");
encodedBytes = streamToArray(istream);

// Get bytes for exected result
refName = "image3ico16x16.png";
refFile = do_get_file(refName);
istream = getFileInputStream(refFile);
do_check_eq(istream.available(), 330);
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
do_check_eq(istream.available(), 1809);

outParam = { value: null };
imgTools.decodeImageData(istream, inMimeType, outParam);
container = outParam.value;

// It's not easy to look at the pixel values from JS, so just
// check the container's size.
do_check_eq(container.width, 32);
do_check_eq(container.height, 32);


/* ========== bug 363986 ========== */
testnum = 363986;
testdesc = "test PNG and JPEG encoders' Read/ReadSegments methods";

var testData = 
    [{preImage: "image3.ico",
      preImageMimeType: "image/x-icon",
      refImage: "image3ico16x16.png",
      refImageMimeType: "image/png"},
     {preImage: "image1.png",
      preImageMimeType: "image/png",
      refImage: "image1png64x64.jpg",
      refImageMimeType: "image/jpeg"}];

for(var i=0; i<testData.length; ++i) {
    var dict = testData[i];

    var imgFile = do_get_file(dict["refImage"]);
    var istream = getFileInputStream(imgFile);
    var refBytes = streamToArray(istream);

    imgFile = do_get_file(dict["preImage"]);
    istream = getFileInputStream(imgFile);

    var outParam = { value: null };
    imgTools.decodeImageData(istream, dict["preImageMimeType"], outParam);
    var container = outParam.value;

    istream = imgTools.encodeImage(container, dict["refImageMimeType"]);

    var sstream = Cc["@mozilla.org/storagestream;1"].
	          createInstance(Ci.nsIStorageStream);
    sstream.init(4096, 4294967295, null);
    var ostream = sstream.getOutputStream(0);
    var bostream = Cc["@mozilla.org/network/buffered-output-stream;1"].
	           createInstance(Ci.nsIBufferedOutputStream);

    //use a tiny buffer to make sure the image data doesn't fully fit in it
    bostream.init(ostream, 8);

    bostream.writeFrom(istream, istream.available());
    bostream.flush(); bostream.close();

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
do_check_eq(istream.available(), 17759);
var errsrc = "none";

try {
  outParam = { value: null };
  imgTools.decodeImageData(istream, inMimeType, outParam);
  container = outParam.value;

  // We should never hit this - decodeImageData throws an assertion because the
  // image decoded doesn't have enough frames.
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

do_check_eq(errsrc, "decode");
checkExpectedError(/NS_ERROR_FAILURE/, err);


/* ========== end ========== */

} catch (e) {
    throw "FAILED in test #" + testnum + " -- " + testdesc + ": " + e;
}
};
