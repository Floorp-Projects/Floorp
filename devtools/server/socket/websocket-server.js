/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { Cc, CC } = require("chrome");
const { executeSoon } = require("devtools/shared/DevToolsUtils");
const { delimitedRead } = require("devtools/shared/transport/stream-utils");
const CryptoHash = CC(
  "@mozilla.org/security/hash;1",
  "nsICryptoHash",
  "initWithString"
);
const threadManager = Cc["@mozilla.org/thread-manager;1"].getService();

// Limit the header size to put an upper bound on allocated memory
const HEADER_MAX_LEN = 8000;

/**
 * Read a line from async input stream and return promise that resolves to the line once
 * it has been read. If the line is longer than HEADER_MAX_LEN, will throw error.
 */
function readLine(input) {
  return new Promise((resolve, reject) => {
    let line = "";
    const wait = () => {
      input.asyncWait(
        stream => {
          try {
            const amountToRead = HEADER_MAX_LEN - line.length;
            line += delimitedRead(input, "\n", amountToRead);

            if (line.endsWith("\n")) {
              resolve(line.trimRight());
              return;
            }

            if (line.length >= HEADER_MAX_LEN) {
              throw new Error(
                `Failed to read HTTP header longer than ${HEADER_MAX_LEN} bytes`
              );
            }

            wait();
          } catch (ex) {
            reject(ex);
          }
        },
        0,
        0,
        threadManager.currentThread
      );
    };

    wait();
  });
}

/**
 * Write a string of bytes to async output stream and return promise that resolves once
 * all data has been written. Doesn't do any utf-16/utf-8 conversion - the string is
 * treated as an array of bytes.
 */
function writeString(output, data) {
  return new Promise((resolve, reject) => {
    const wait = () => {
      if (data.length === 0) {
        resolve();
        return;
      }

      output.asyncWait(
        stream => {
          try {
            const written = output.write(data, data.length);
            data = data.slice(written);
            wait();
          } catch (ex) {
            reject(ex);
          }
        },
        0,
        0,
        threadManager.currentThread
      );
    };

    wait();
  });
}

/**
 * Read HTTP request from async input stream.
 * @return Request line (string) and Map of header names and values.
 */
const readHttpRequest = async function(input) {
  let requestLine = "";
  const headers = new Map();

  while (true) {
    const line = await readLine(input);
    if (!line.length) {
      break;
    }

    if (!requestLine) {
      requestLine = line;
    } else {
      const colon = line.indexOf(":");
      if (colon == -1) {
        throw new Error(`Malformed HTTP header: ${line}`);
      }

      const name = line.slice(0, colon).toLowerCase();
      const value = line.slice(colon + 1).trim();
      headers.set(name, value);
    }
  }

  return { requestLine, headers };
};

/**
 * Write HTTP response (array of strings) to async output stream.
 */
function writeHttpResponse(output, response) {
  const responseString = response.join("\r\n") + "\r\n\r\n";
  return writeString(output, responseString);
}

/**
 * Process the WebSocket handshake headers and return the key to be sent in
 * Sec-WebSocket-Accept response header.
 */
function processRequest({ requestLine, headers }) {
  const [method, path] = requestLine.split(" ");
  if (method !== "GET") {
    throw new Error("The handshake request must use GET method");
  }

  if (path !== "/") {
    throw new Error("The handshake request has unknown path");
  }

  const upgrade = headers.get("upgrade");
  if (!upgrade || upgrade !== "websocket") {
    throw new Error("The handshake request has incorrect Upgrade header");
  }

  const connection = headers.get("connection");
  if (
    !connection ||
    !connection
      .split(",")
      .map(t => t.trim())
      .includes("Upgrade")
  ) {
    throw new Error("The handshake request has incorrect Connection header");
  }

  const version = headers.get("sec-websocket-version");
  if (!version || version !== "13") {
    throw new Error(
      "The handshake request must have Sec-WebSocket-Version: 13"
    );
  }

  // Compute the accept key
  const key = headers.get("sec-websocket-key");
  if (!key) {
    throw new Error(
      "The handshake request must have a Sec-WebSocket-Key header"
    );
  }

  return { acceptKey: computeKey(key) };
}

function computeKey(key) {
  const str = key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

  const data = Array.from(str, ch => ch.charCodeAt(0));
  const hash = new CryptoHash("sha1");
  hash.update(data, data.length);
  return hash.finish(true);
}

/**
 * Perform the server part of a WebSocket opening handshake on an incoming connection.
 */
const serverHandshake = async function(input, output) {
  // Read the request
  const request = await readHttpRequest(input);

  try {
    // Check and extract info from the request
    const { acceptKey } = processRequest(request);

    // Send response headers
    await writeHttpResponse(output, [
      "HTTP/1.1 101 Switching Protocols",
      "Upgrade: websocket",
      "Connection: Upgrade",
      `Sec-WebSocket-Accept: ${acceptKey}`,
    ]);
  } catch (error) {
    // Send error response in case of error
    await writeHttpResponse(output, ["HTTP/1.1 400 Bad Request"]);
    throw error;
  }
};

/**
 * Accept an incoming WebSocket server connection.
 * Takes an established nsISocketTransport in the parameters.
 * Performs the WebSocket handshake and waits for the WebSocket to open.
 * Returns Promise with a WebSocket ready to send and receive messages.
 */
const accept = async function(transport, input, output) {
  await serverHandshake(input, output);

  const transportProvider = {
    setListener(upgradeListener) {
      // The onTransportAvailable callback shouldn't be called synchronously.
      executeSoon(() => {
        upgradeListener.onTransportAvailable(transport, input, output);
      });
    },
  };

  return new Promise((resolve, reject) => {
    const socket = WebSocket.createServerWebSocket(
      null,
      [],
      transportProvider,
      ""
    );
    socket.addEventListener("close", () => {
      input.close();
      output.close();
    });

    socket.onopen = () => resolve(socket);
    socket.onerror = err => reject(err);
  });
};

exports.accept = accept;
