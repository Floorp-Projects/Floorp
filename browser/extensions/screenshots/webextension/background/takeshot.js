/* globals communication, shot, main, auth, catcher, analytics, buildSettings, blobConverters */

"use strict";

this.takeshot = (function() {
  let exports = {};
  const Shot = shot.AbstractShot;
  const { sendEvent } = analytics;

  communication.register("takeShot", catcher.watchFunction((sender, options) => {
    let { captureType, captureText, scroll, selectedPos, shotId, shot, imageBlob } = options;
    shot = new Shot(main.getBackend(), shotId, shot);
    shot.favicon = sender.tab.favIconUrl;
    let capturePromise = Promise.resolve();
    let openedTab;
    if (!shot.clipNames().length) {
      // canvas.drawWindow isn't available, so we fall back to captureVisibleTab
      capturePromise = screenshotPage(selectedPos, scroll).then((dataUrl) => {
        shot.addClip({
          createdDate: Date.now(),
          image: {
            url: "data:",
            captureType,
            text: captureText,
            location: selectedPos,
            dimensions: {
              x: selectedPos.right - selectedPos.left,
              y: selectedPos.bottom - selectedPos.top
            }
          }
        });
      });
    }
    let convertBlobPromise = Promise.resolve();
    if (buildSettings.uploadBinary && !imageBlob) {
      imageBlob = blobConverters.dataUrlToBlob(shot.getClip(shot.clipNames()[0]).image.url);
      shot.getClip(shot.clipNames()[0]).image.url = "";
    } else if (!buildSettings.uploadBinary && imageBlob) {
      convertBlobPromise = blobConverters.blobToDataUrl(imageBlob).then((dataUrl) => {
        shot.getClip(shot.clipNames()[0]).image.url = dataUrl;
      });
      imageBlob = null;
    }
    let shotAbTests = {};
    let abTests = auth.getAbTests();
    for (let testName of Object.keys(abTests)) {
      if (abTests[testName].shotField) {
        shotAbTests[testName] = abTests[testName].value;
      }
    }
    if (Object.keys(shotAbTests).length) {
      shot.abTests = shotAbTests;
    }
    return catcher.watchPromise(capturePromise.then(() => {
      return convertBlobPromise;
    }).then(() => {
      return browser.tabs.create({url: shot.creatingUrl})
    }).then((tab) => {
      openedTab = tab;
      return uploadShot(shot, imageBlob);
    }).then(() => {
      return browser.tabs.update(openedTab.id, {url: shot.viewUrl}).then(
        null,
        (error) => {
          // FIXME: If https://bugzilla.mozilla.org/show_bug.cgi?id=1365718 is resolved,
          // use the errorCode added as an additional check:
          if ((/invalid tab id/i).test(error)) {
            // This happens if the tab was closed before the upload completed
            return browser.tabs.create({url: shot.viewUrl});
          }
          throw error;
        }
      );
    }).then(() => {
      return shot.viewUrl;
    }).catch((error) => {
      browser.tabs.remove(openedTab.id);
      throw error;
    }));
  }));

  communication.register("screenshotPage", (sender, selectedPos, scroll) => {
    return screenshotPage(selectedPos, scroll);
  });

  function screenshotPage(pos, scroll) {
    pos = {
      top: pos.top - scroll.scrollY,
      left: pos.left - scroll.scrollX,
      bottom: pos.bottom - scroll.scrollY,
      right: pos.right - scroll.scrollX
    };
    pos.width = pos.right - pos.left;
    pos.height = pos.bottom - pos.top;
    return catcher.watchPromise(browser.tabs.captureVisibleTab(
      null,
      {format: "png"}
    ).then((dataUrl) => {
      let image = new Image();
      image.src = dataUrl;
      return new Promise((resolve, reject) => {
        image.onload = catcher.watchFunction(() => {
          let xScale = image.width / scroll.innerWidth;
          let yScale = image.height / scroll.innerHeight;
          let canvas = document.createElement("canvas");
          canvas.height = pos.height * yScale;
          canvas.width = pos.width * xScale;
          let context = canvas.getContext("2d");
          context.drawImage(
            image,
            pos.left * xScale, pos.top * yScale,
            pos.width * xScale, pos.height * yScale,
            0, 0,
            pos.width * xScale, pos.height * yScale
          );
          let result = canvas.toDataURL();
          resolve(result);
        });
      });
    }));
  }

  /** Combines two buffers or Uint8Array's */
  function concatBuffers(buffer1, buffer2) {
    var tmp = new Uint8Array(buffer1.byteLength + buffer2.byteLength);
    tmp.set(new Uint8Array(buffer1), 0);
    tmp.set(new Uint8Array(buffer2), buffer1.byteLength);
    return tmp.buffer;
  }

  /** Creates a multipart TypedArray, given {name: value} fields
      and {name: blob} files

      Returns {body, "content-type"}
      */
  function createMultipart(fields, fileField, fileFilename, blob) {
    let boundary = "---------------------------ScreenshotBoundary" + Date.now();
    return blobConverters.blobToArray(blob).then((blobAsBuffer) => {
      let body = [];
      for (let name in fields) {
        body.push("--" + boundary);
        body.push(`Content-Disposition: form-data; name="${name}"`);
        body.push("");
        body.push(fields[name]);
      }
      body.push("--" + boundary);
      body.push(`Content-Disposition: form-data; name="${fileField}"; filename="${fileFilename}"`);
      body.push(`Content-Type: ${blob.type}`);
      body.push("");
      body.push("");
      body = body.join("\r\n");
      let enc = new TextEncoder("utf-8");
      body = enc.encode(body);
      body = concatBuffers(body.buffer, blobAsBuffer);
      let tail = "\r\n" + "--" + boundary + "--";
      tail = enc.encode(tail);
      body = concatBuffers(body, tail.buffer);
      return {
        "content-type": `multipart/form-data; boundary=${boundary}`,
        body
      };
    });
  }

  function uploadShot(shot, blob) {
    let headers;
    return auth.authHeaders().then((_headers) => {
      headers = _headers;
      if (blob) {
        return createMultipart(
          {shot: JSON.stringify(shot.asJson())},
          "blob", "screenshot.png", blob
        );
      } else {
        return {
          "content-type": "application/json",
          body: JSON.stringify(shot.asJson())
        };
      }
    }).then((submission) => {
      headers["content-type"] = submission["content-type"];
      sendEvent("upload", "started", {eventValue: Math.floor(submission.body.length / 1000)});
      return fetch(shot.jsonUrl, {
        method: "PUT",
        mode: "cors",
        headers,
        body: submission.body
      });
    }).then((resp) => {
      if (!resp.ok) {
        sendEvent("upload-failed", `status-${resp.status}`);
        let exc = new Error(`Response failed with status ${resp.status}`);
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
