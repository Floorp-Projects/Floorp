/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);

const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs"
);

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs"
);

const ImgTools = Cc["@mozilla.org/image/tools;1"].getService(
  Ci.imgITools
) as imgITools;

export const ImageTools = {
  loadImage(
    dataURI: nsIURI
  ): Promise<{ type: string; container: imgIContainer }> {
    return new Promise((resolve, reject) => {
      if (!dataURI.schemeIs("data")) {
        reject(new Error("Should only be loading data URIs."));
        return;
      }

      const channel = NetUtil.newChannel({
        uri: dataURI,
        loadUsingSystemPrincipal: true,
      });

      ImgTools.decodeImageFromChannelAsync(
        dataURI,
        channel,
        (container: imgIContainer, aStatus: number) => {
          if (Components.isSuccessCode(aStatus)) {
            resolve({
              type: channel.contentType,
              container,
            });
          } else {
            reject(Components.Exception("Failed to load image.", aStatus));
          }
        },
        null as unknown as imgINotificationObserver
      );
    });
  },

  scaleImage(container: imgIContainer, width: number, height: number) {
    return new Promise((resolve, reject) => {
      const stream = ImgTools.encodeScaledImage(
        container,
        "image/png",
        width,
        height,
        ""
      );

      try {
        // biome-ignore lint/suspicious/noExplicitAny: <explanation>
        (stream as unknown as any).QueryInterface(Ci.nsIAsyncInputStream);
      } catch (e) {
        reject(
          Components.Exception(
            "imgIEncoder must implement nsIAsyncInputStream",
            e
          )
        );
      }

      const binaryStream = Cc[
        "@mozilla.org/binaryinputstream;1"
      ].createInstance(Ci.nsIBinaryInputStream);

      binaryStream.setInputStream(stream);

      const buffers: BlobPart[] | undefined = [];
      const callback = () => {
        try {
          const available = binaryStream.available();
          if (available) {
            const buffer = new ArrayBuffer(available);
            binaryStream.readArrayBuffer(available, buffer);
            buffers.push(buffer);

            (stream as unknown as nsIAsyncInputStream).asyncWait(
              callback,
              0,
              0,
              Services.tm.mainThread
            );
            return;
          }

          // No data available, assume the encoding is done.
          resolve(new Blob(buffers));
        } catch (e) {
          reject(e);
        }
      };

      try {
        (stream as unknown as nsIAsyncInputStream).asyncWait(
          callback,
          0,
          0,
          Services.tm.mainThread
        );
      } catch (e) {
        reject(e);
      }
    });
  },

  saveIcon(
    container: imgIContainer,
    width: number,
    height: number,
    target: nsIFile
  ) {
    let format: string;
    if (AppConstants.platform === "win") {
      format = "image/vnd.microsoft.icon";
    }
    if (AppConstants.platform === "linux") {
      format = "image/png";
    }
    return new Promise<void>((resolve, reject) => {
      const output = FileUtils.openFileOutputStream(target);
      const stream = ImgTools.encodeScaledImage(
        container,
        format,
        width,
        height,
        ""
      );
      NetUtil.asyncCopy(stream, output, (aStatus: number) => {
        if (Components.isSuccessCode(aStatus)) {
          resolve();
        } else {
          reject(Components.Exception("Failed to save icon.", aStatus));
        }
      });
    });
  },

  decodeImageFromArrayBuffer(arrayBuffer: ArrayBuffer, contentType: string) {
    return ImgTools.decodeImageFromArrayBuffer(arrayBuffer, contentType);
  },
};
