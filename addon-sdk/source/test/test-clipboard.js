/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

require("sdk/clipboard");

const { Cc, Ci } = require("chrome");

const imageTools = Cc["@mozilla.org/image/tools;1"].
                    getService(Ci.imgITools);

const io = Cc["@mozilla.org/network/io-service;1"].
                    getService(Ci.nsIIOService);

const base64png = "data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAACAAAAAgCAYA" +
                  "AABzenr0AAAASUlEQVRYhe3O0QkAIAwD0eyqe3Q993AQ3cBSUKpygfsNTy" +
                  "N5ugbQpK0BAADgP0BRDWXWlwEAAAAAgPsA3rzDaAAAAHgPcGrpgAnzQ2FG" +
                  "bWRR9AAAAABJRU5ErkJggg%3D%3D";

const { base64jpeg } = require("./fixtures");

const canvasHTML = "data:text/html," + encodeURIComponent(
  "<html>\
    <body>\
      <canvas width='32' height='32'></canvas>\
    </body>\
  </html>"
);

function comparePixelImages(imageA, imageB, callback) {
  let tabs = require("sdk/tabs");

  tabs.open({
    url: canvasHTML,

    onReady: function onReady(tab) {
      let worker = tab.attach({
        contentScript: "new " + function() {
          let canvas = document.querySelector("canvas");
          let context = canvas.getContext("2d");

          self.port.on("draw-image", function(imageURI) {
            let img = new Image();

            img.onload = function() {
              context.drawImage(this, 0, 0);

              let pixels = Array.join(context.getImageData(0, 0, 32, 32).data);
              self.port.emit("image-pixels", pixels);
            }

            img.src = imageURI;
          });
        }
      });

      let compared = "";

      worker.port.on("image-pixels", function (pixels) {
        if (!compared) {
          compared = pixels;
          this.emit("draw-image", imageB);
        } else {
          tab.close(callback.bind(null, compared === pixels))
        }
      });

      worker.port.emit("draw-image", imageA);
    }
  });
}


// Test the typical use case, setting & getting with no flavors specified
exports["test With No Flavor"] = function(assert) {
  var contents = "hello there";
  var flavor = "text";
  var fullFlavor = "text/unicode";
  var clip = require("sdk/clipboard");

  // Confirm we set the clipboard
  assert.ok(clip.set(contents));

  // Confirm flavor is set
  assert.equal(clip.currentFlavors[0], flavor);

  // Confirm we set the clipboard
  assert.equal(clip.get(), contents);

  // Confirm we can get the clipboard using the flavor
  assert.equal(clip.get(flavor), contents);

  // Confirm we can still get the clipboard using the full flavor
  assert.equal(clip.get(fullFlavor), contents);
};

// Test the slightly less common case where we specify the flavor
exports["test With Flavor"] = function(assert) {
  var contents = "<b>hello there</b>";
  var contentsText = "hello there";
  var flavor = "html";
  var fullFlavor = "text/html";
  var unicodeFlavor = "text";
  var unicodeFullFlavor = "text/unicode";
  var clip = require("sdk/clipboard");

  assert.ok(clip.set(contents, flavor));

  assert.equal(clip.currentFlavors[0], unicodeFlavor);
  assert.equal(clip.currentFlavors[1], flavor);
  assert.equal(clip.get(), contentsText);
  assert.equal(clip.get(flavor), contents);
  assert.equal(clip.get(fullFlavor), contents);
  assert.equal(clip.get(unicodeFlavor), contentsText);
  assert.equal(clip.get(unicodeFullFlavor), contentsText);
};

// Test that the typical case still works when we specify the flavor to set
exports["test With Redundant Flavor"] = function(assert) {
  var contents = "<b>hello there</b>";
  var flavor = "text";
  var fullFlavor = "text/unicode";
  var clip = require("sdk/clipboard");

  assert.ok(clip.set(contents, flavor));
  assert.equal(clip.currentFlavors[0], flavor);
  assert.equal(clip.get(), contents);
  assert.equal(clip.get(flavor), contents);
  assert.equal(clip.get(fullFlavor), contents);
};

exports["test Not In Flavor"] = function(assert) {
  var contents = "hello there";
  var flavor = "html";
  var clip = require("sdk/clipboard");

  assert.ok(clip.set(contents));
  // If there's nothing on the clipboard with this flavor, should return null
  assert.equal(clip.get(flavor), null);
};

exports["test Set Image"] = function(assert) {
  var clip = require("sdk/clipboard");
  var flavor = "image";
  var fullFlavor = "image/png";

  assert.ok(clip.set(base64png, flavor), "clipboard set");
  assert.equal(clip.currentFlavors[0], flavor, "flavor is set");
};

exports["test Get Image"] = function(assert, done) {
  var clip = require("sdk/clipboard");

  clip.set(base64png, "image");

  var contents = clip.get();

  comparePixelImages(base64png, contents, function (areEquals) {
    assert.ok(areEquals,
      "Image gets from clipboard equals to image sets to the clipboard");

    done();
  });
}

exports["test Set Image Type Not Supported"] = function(assert) {
  var clip = require("sdk/clipboard");
  var flavor = "image";

  assert.throws(function () {
    clip.set(base64jpeg, flavor);
  }, "Invalid flavor for image/jpeg");

};

// Notice that `imageTools.decodeImageData`, used by `clipboard.set` method for
// images, write directly to the javascript console the error in case the image
// is corrupt, even if the error is catched.
//
// See: http://mxr.mozilla.org/mozilla-central/source/image/src/Decoder.cpp#136
exports["test Set Image Type Wrong Data"] = function(assert) {
  var clip = require("sdk/clipboard");
  var flavor = "image";

  var wrongPNG = "data:image/png" + base64jpeg.substr(15);

  assert.throws(function () {
    clip.set(wrongPNG, flavor);
  }, "Unable to decode data given in a valid image.");
};

require("sdk/test").run(exports)
