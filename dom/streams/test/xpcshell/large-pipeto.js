const { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

// Stamp an array buffer with a pattern; verified by verify_chunk below.
function init(array) {
  for (var i = 0; i < array.length; i++) {
    array[i] = i % 256;
  }
}

// The construction of the file below ends up with 12 instances
// of the array buffer -- we want this to be larger than 2**32 bytes
// to exercise potential truncation of nsIInputStream::Read's count
// parameter.
const ABLENGTH = (2 ** 32 + 8) / 12;

// Get a large file (bigger than 2GB!)
function get_file() {
  const array = new ArrayBuffer(ABLENGTH);
  const buff = new Uint8Array(array);

  // Stamp with pattern.
  init(buff);

  const blob = new Blob([buff, buff], {});
  return new File([blob, blob, blob, blob, blob, blob], {});
}

// Verify that the chunks the stream recieve correspond to the initialization
function verify_chunk(chunk, verification_state) {
  for (var j = 0; j < chunk.length; j++) {
    // If we don't match the fill pattern.
    if (chunk[j] != verification_state.expected) {
      // we ran out of array buffer; so we should be looking at the first byte of the array again.
      if (
        verification_state.total_index % ABLENGTH != 0 ||
        chunk[j] != 0 /* ASSUME THAT THE INITIALIZATION OF THE BUFFER IS ZERO */
      ) {
        throw new Error(
          `Mismatch: chunk[${j}] (${chunk[j]}) != ${verification_state.expected} (total_index ${verification_state.total_index})`
        );
      }
      // Reset the fill expectation to 1 for next round.
      verification_state.expected = 1;
    } else {
      // We are inside regular fill section
      verification_state.expected = (verification_state.expected + 1) % 256;
    }
    verification_state.total_index++;
  }
}

// Pipe To Testing: Win32 can't handle the file size created in this test and OOMs.
add_task(
  {
    skip_if: () => AppConstants.platform == "win" && !Services.appinfo.is64Bit,
  },
  async () => {
    var chunk_verification_state = {
      expected: 0,
      total_index: 0,
    };

    const file = get_file();

    await file.stream().pipeTo(
      new WritableStream({
        write(chunk) {
          verify_chunk(chunk, chunk_verification_state);
        },
      })
    );
  }
);

// Do the same test as above, but this time don't use pipeTo.
add_task(
  {
    skip_if: () => AppConstants.platform == "win" && !Services.appinfo.is64Bit,
  },
  async () => {
    var file = get_file();

    var chunk_verification_state = {
      expected: 0,
      total_index: 0,
    };

    var streamReader = file.stream().getReader();

    while (true) {
      var res = await streamReader.read();
      if (res.done) {
        break;
      }
      var chunk = res.value;
      verify_chunk(chunk, chunk_verification_state);
    }
  }
);
