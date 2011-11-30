function parseQuery(request, key) {
  var params = request.queryString.split('&');
  for (var j = 0; j < params.length; ++j) {
    var p = params[j];
	if (p == key)
	  return true;
    if (p.indexOf(key + "=") == 0)
	  return p.substring(key.length + 1);
	if (p.indexOf("=") < 0 && key == "")
	  return p;
  }
  return false;
}

function push32BE(array, input) {
  array.push(String.fromCharCode((input >> 24) & 0xff));
  array.push(String.fromCharCode((input >> 16) & 0xff));
  array.push(String.fromCharCode((input >> 8) & 0xff));
  array.push(String.fromCharCode((input) & 0xff));
}

function push32LE(array, input) {
  array.push(String.fromCharCode((input) & 0xff));
  array.push(String.fromCharCode((input >> 8) & 0xff));
  array.push(String.fromCharCode((input >> 16) & 0xff));
  array.push(String.fromCharCode((input >> 24) & 0xff));
}

function push16LE(array, input) {
  array.push(String.fromCharCode((input) & 0xff));
  array.push(String.fromCharCode((input >> 8) & 0xff));
}

function buildWave(samples, sample_rate) {
  const RIFF_MAGIC = 0x52494646;
  const WAVE_MAGIC = 0x57415645;
  const FRMT_MAGIC = 0x666d7420;
  const DATA_MAGIC = 0x64617461;
  const RIFF_SIZE = 44;

  var header = [];
  push32BE(header, RIFF_MAGIC);
  push32LE(header, RIFF_SIZE + samples.length * 2);
  push32BE(header, WAVE_MAGIC);
  push32BE(header, FRMT_MAGIC);
  push32LE(header, 16);
  push16LE(header, 1);
  push16LE(header, 1);
  push32LE(header, sample_rate);
  push32LE(header, sample_rate);
  push16LE(header, 2);
  push16LE(header, 16);
  push32BE(header, DATA_MAGIC);
  push32LE(header, samples.length * 2);
  for (var i = 0; i < samples.length; ++i) {
    push16LE(header, samples[i], 2);
  }
  return header;
}

const Ci = Components.interfaces;
const CC = Components.Constructor;
const Timer = CC("@mozilla.org/timer;1", "nsITimer", "initWithCallback");
const BinaryOutputStream = CC("@mozilla.org/binaryoutputstream;1",
                             "nsIBinaryOutputStream",
                             "setOutputStream");

function poll(f) {
  if (f()) {
    return;
  }
  new Timer(function() { poll(f); }, 100, Ci.nsITimer.TYPE_ONE_SHOT);
}

function handleRequest(request, response)
{
  var cancel = parseQuery(request, "cancelkey");
  if (cancel) {
    setState(cancel[1], "cancelled");
    response.setStatusLine(request.httpVersion, 200, "OK");
    response.write("Cancel approved!");
    return;
  }

  var samples = [];
  for (var i = 0; i < 100000; ++i) {
    samples.push(0);
  }
  var bytes = buildWave(samples, 44100).join("");

  var key = parseQuery(request, "key");
  response.setHeader("Content-Type", "audio/x-wav");
  response.setHeader("Content-Length", ""+bytes.length, false);

  var out = new BinaryOutputStream(response.bodyOutputStream);

  var start = 0, end = bytes.length - 1;
  if (request.hasHeader("Range"))
  {
    var rangeMatch = request.getHeader("Range").match(/^bytes=(\d+)?-(\d+)?$/);

    if (rangeMatch[1] !== undefined)
      start = parseInt(rangeMatch[1], 10);

    if (rangeMatch[2] !== undefined)
      end = parseInt(rangeMatch[2], 10);

    // No start given, so the end is really the count of bytes from the
    // end of the file.
    if (start === undefined)
    {
      start = Math.max(0, bytes.length - end);
      end   = bytes.length - 1;
    }

    // start and end are inclusive
    if (end === undefined || end >= bytes.length)
      end = bytes.length - 1;

    if (end < start)
    {
      response.setStatusLine(request.httpVersion, 200, "OK");
      start = 0;
      end = bytes.length - 1;
    }
    else
    {
      response.setStatusLine(request.httpVersion, 206, "Partial Content");
      var contentRange = "bytes " + start + "-" + end + "/" + bytes.length;
      response.setHeader("Content-Range", contentRange);
    }
  }
  
  if (start > 0) {
    // Send all requested data
    out.write(bytes.slice(start, end + 1), end + 1 - start);
    return;
  }

  // Write the first 120K of the Wave file. We know the cache size is set to
  // 100K so this will fill the cache and and cause a "suspend" event on
  // the loading element.
  out.write(bytes, 120000);

  response.processAsync();
  // Now wait for the message to cancel this response
  poll(function() {
    if (getState(key[1]) != "cancelled") {
      return false;
    }
    response.finish();
    return true;
  });
}
