/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* globals communication, shot, main, auth, catcher, analytics, buildSettings, blobConverters, thumbnailGenerator */

"use strict";

this.takeshot = (function() {
  const exports = {};
  const Shot = shot.AbstractShot;
  const { sendEvent, incrementCount } = analytics;
  const MAX_CANVAS_DIMENSION = 32767;

  communication.register("screenshotPage", (sender, selectedPos, isFullPage, devicePixelRatio) => {
    return screenshotPage(selectedPos, isFullPage, devicePixelRatio);
  });

  function screenshotPage(pos, isFullPage, devicePixelRatio) {
    pos.width = Math.min(pos.right - pos.left, MAX_CANVAS_DIMENSION);
    pos.height = Math.min(pos.bottom - pos.top, MAX_CANVAS_DIMENSION);

    // If we are printing the full page or a truncated full page,
    // we must pass in this rectangle to preview the entire image
    let options = {format: "png"};
    if (isFullPage) {
      let rectangle = {
        x: 0,
        y: 0,
        width: pos.width,
        height: pos.height,
      }
      options.rect = rectangle;

      // To avoid creating extremely large images (which causes
      // performance problems), we set the scale to 1.
      devicePixelRatio = options.scale = 1;
    } else {
      let rectangle = {
        x: pos.left,
        y: pos.top,
        width: pos.width,
        height: pos.height,
      }
      options.rect = rectangle
    }

    return catcher.watchPromise(browser.tabs.captureTab(
      null,
      options,
    ).then((dataUrl) => {
      const image = new Image();
      image.src = dataUrl;
      return new Promise((resolve, reject) => {
        image.onload = catcher.watchFunction(() => {
          const xScale = devicePixelRatio;
          const yScale = devicePixelRatio;
          const canvas = document.createElement("canvas");
          canvas.height = pos.height * yScale;
          canvas.width = pos.width * xScale;
          const context = canvas.getContext("2d");
          context.drawImage(
            image,
            0, 0,
            pos.width * xScale, pos.height * yScale,
            0, 0,
            pos.width * xScale, pos.height * yScale
          );
          const result = canvas.toDataURL();
          resolve(result);
        });
      });
    }));
  }

  /** Combines two buffers or Uint8Array's */
  function concatBuffers(buffer1, buffer2) {
    const tmp = new Uint8Array(buffer1.byteLength + buffer2.byteLength);
    tmp.set(new Uint8Array(buffer1), 0);
    tmp.set(new Uint8Array(buffer2), buffer1.byteLength);
    return tmp.buffer;
  }

  /** Creates a multipart TypedArray, given {name: value} fields
      and a files array in the format of
      [{fieldName: "NAME", filename: "NAME.png", blob: fileBlob}, {...}, ...]

      Returns {body, "content-type"}
      */
  function createMultipart(fields, files) {
    const boundary = "---------------------------ScreenshotBoundary" + Date.now();
    let body = [];
    for (const name in fields) {
      body.push("--" + boundary);
      body.push(`Content-Disposition: form-data; name="${name}"`);
      body.push("");
      body.push(fields[name]);
    }
    body.push("");
    body = body.join("\r\n");
    const enc = new TextEncoder("utf-8");
    body = enc.encode(body).buffer;

    const blobToArrayPromises = files.map(f => {
      return blobConverters.blobToArray(f.blob);
    });

    return Promise.all(blobToArrayPromises).then(buffers => {
      for (let i = 0; i < buffers.length; i++) {
        let filePart = [];
        filePart.push("--" + boundary);
        filePart.push(`Content-Disposition: form-data; name="${files[i].fieldName}"; filename="${files[i].filename}"`);
        filePart.push(`Content-Type: ${files[i].blob.type}`);
        filePart.push("");
        filePart.push("");
        filePart = filePart.join("\r\n");
        filePart = concatBuffers(enc.encode(filePart).buffer, buffers[i]);
        body = concatBuffers(body, filePart);
        body = concatBuffers(body, enc.encode("\r\n").buffer);
      }

      let tail = `\r\n--${boundary}--`;
      tail = enc.encode(tail);
      body = concatBuffers(body, tail.buffer);
      return {
        "content-type": `multipart/form-data; boundary=${boundary}`,
        body,
      };
    });
  }

  function uploadShot(shot, blob, thumbnail) {
    let headers;
    return auth.authHeaders().then((_headers) => {
      headers = _headers;
      if (blob) {
        const files = [ {fieldName: "blob", filename: "screenshot.png", blob} ];
        if (thumbnail) {
          files.push({fieldName: "thumbnail", filename: "thumbnail.png", blob: thumbnail});
        }
        return createMultipart(
          {shot: JSON.stringify(shot)},

          files
        );
      }
      return {
        "content-type": "application/json",
        body: JSON.stringify(shot),
      };

    }).then((submission) => {
      headers["content-type"] = submission["content-type"];
      sendEvent("upload", "started", {eventValue: Math.floor(submission.body.length / 1000)});
      return fetch(shot.jsonUrl, {
        method: "PUT",
        mode: "cors",
        headers,
        body: submission.body,
      });
    }).then((resp) => {
      if (!resp.ok) {
        sendEvent("upload-failed", `status-${resp.status}`);
        const exc = new Error(`Response failed with status ${resp.status}`);
        exc.popupMessage = "REQUEST_ERROR";
        throw exc;
      } else {
        sendEvent("upload", "success");
      }
    }, (error) => {
      // FIXME: I'm not sure what exceptions we can expect
      sendEvent("upload-failed", "connection");
      error.popupMessage = "CONNECTION_ERROR";
      throw error;
    });
  }

  return exports;
})();
