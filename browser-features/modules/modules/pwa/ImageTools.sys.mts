/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs",
);

const { FileUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/FileUtils.sys.mjs",
);

const { NetUtil } = ChromeUtils.importESModule(
  "resource://gre/modules/NetUtil.sys.mjs",
);

const ImgTools = Cc["@mozilla.org/image/tools;1"].getService(
  Ci.imgITools,
) as imgITools;

export const ImageTools = {
  loadImage(
    dataURI: nsIURI,
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
        null as unknown as imgINotificationObserver,
      );
    });
  },

  scaleImage(container: imgIContainer, width: number, height: number) {
    return new Promise((resolve, reject) => {
      let stream: nsIInputStream;
      try {
        if (container.type === Ci.imgIContainer.TYPE_VECTOR) {
          stream = ImgTools.encodeImage(container, "image/png", "");
        } else {
          stream = ImgTools.encodeScaledImage(
            container,
            "image/png",
            width,
            height,
            "",
          );
        }
      } catch (vectorErr) {
        console.warn("ImageTools: failed initial encode, retrying", vectorErr);
        try {
          stream = ImgTools.encodeImage(container, "image/png", "");
        } catch (e2) {
          console.error("ImageTools: failed fallback encode", e2);
          reject(e2);
          return;
        }
      }

      try {
        (
          stream as unknown as { QueryInterface: (iid: unknown) => unknown }
        ).QueryInterface(Ci.nsIAsyncInputStream);
      } catch (err) {
        console.error(
          "ImageTools: stream does not implement nsIAsyncInputStream",
          err,
        );
        reject(
          Components.Exception(
            "imgIEncoder must implement nsIAsyncInputStream",
            Components.results.NS_ERROR_FAILURE,
          ),
        );
        return;
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
              Services.tm.mainThread,
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
          Services.tm.mainThread,
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
    target: nsIFile,
  ) {
    let format = "image/png";
    if (AppConstants.platform === "win") {
      format = "image/vnd.microsoft.icon";
    }
    return new Promise<void>((resolve, reject) => {
      const output = FileUtils.openFileOutputStream(target);
      let stream: nsIInputStream;
      try {
        if (container.type === Ci.imgIContainer.TYPE_VECTOR) {
          stream = ImgTools.encodeImage(container, format, "");
        } else {
          stream = ImgTools.encodeScaledImage(
            container,
            format,
            width,
            height,
            "",
          );
        }
      } catch (vectorErr) {
        console.warn("ImageTools: failed initial encode, retrying", vectorErr);
        try {
          stream = ImgTools.encodeImage(container, format, "");
        } catch (e2) {
          console.error("ImageTools: failed fallback encode", e2);
          reject(e2);
          return;
        }
      }
      NetUtil.asyncCopy(stream, output, (aStatus: number) => {
        if (Components.isSuccessCode(aStatus)) {
          resolve();
        } else {
          reject(Components.Exception("Failed to save icon.", aStatus));
        }
      });
    });
  },

  async saveDataURI(dataURI: nsIURI, target: nsIFile) {
    if (!dataURI.schemeIs("data")) {
      throw new Error("saveDataURI only supports data: URIs.");
    }

    await IOUtils.makeDirectory(target.parent!.path, {
      createAncestors: true,
      ignoreExisting: true,
    });

    return new Promise<void>((resolve, reject) => {
      try {
        const channel = NetUtil.newChannel({
          uri: dataURI,
          loadUsingSystemPrincipal: true,
        });
        const stream = channel.open();
        const output = FileUtils.openFileOutputStream(target);
        NetUtil.asyncCopy(stream, output, (status: number) => {
          if (Components.isSuccessCode(status)) {
            resolve();
          } else {
            reject(
              Components.Exception("Failed to save raw icon data.", status),
            );
          }
        });
      } catch (e) {
        reject(e);
      }
    });
  },

  async saveIconForPlatform(
    sourceURI: nsIURI,
    targetFile: nsIFile,
    width: number,
    height: number,
  ): Promise<string | null> {
    const { container, type } = await ImageTools.loadImage(sourceURI);
    try {
      // For Linux, try to save as SVG if available for better scaling
      if (
        AppConstants.platform === "linux" &&
        type?.includes("svg") &&
        container.type === Ci.imgIContainer.TYPE_VECTOR
      ) {
        const svgFile = targetFile.clone();
        const leafName = svgFile.leafName;
        if (leafName.endsWith(".png")) {
          svgFile.leafName = leafName.replace(/\.png$/, ".svg");
        }
        await ImageTools.saveDataURI(sourceURI, svgFile);
        return svgFile.path;
      }

      await ImageTools.saveIcon(container, width, height, targetFile);
      return targetFile.path;
    } catch (error) {
      console.warn("ImageTools: saveIcon failed, attempting fallback", error);
      if (type?.includes("svg")) {
        const svgFile = targetFile.clone();
        const leafName = svgFile.leafName;
        if (leafName.endsWith(".png")) {
          svgFile.leafName = leafName.replace(/\.png$/, ".svg");
        } else if (!leafName.endsWith(".svg")) {
          svgFile.leafName = `${leafName}.svg`;
        }
        await ImageTools.saveDataURI(sourceURI, svgFile);
        return svgFile.path;
      }
      throw error;
    }
  },

  decodeImageFromArrayBuffer(arrayBuffer: ArrayBuffer, contentType: string) {
    return ImgTools.decodeImageFromArrayBuffer(arrayBuffer, contentType);
  },
};
