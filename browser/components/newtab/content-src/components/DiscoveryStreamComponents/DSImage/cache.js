// This cache is for tracking queued image source requests from DSImage instances and leveraging
//  larger sizes of images to scale down in the browser instead of making additional
//  requests for smaller sizes from the server.

let cache = {
  query(url, size, set) {
    // Create an empty set if none exists
    // Need multiple cache sets because the browser decides what set to use based on pixel density
    if (this.queuedImages[set] === undefined) {
      this.queuedImages[set] = {};
    }

    let sizeToRequest; // The px width to request from Thumbor via query value

    if (!this.queuedImages[set][url] || this.queuedImages[set][url] < size) {
      this.queuedImages[set][url] = size;
      sizeToRequest = size;
    } else {
      // Use the larger size already queued for download (and allow browser to scale down)
      sizeToRequest = this.queuedImages[set][url];
    }

    return sizeToRequest;
  },
  queuedImages: {},
};

export {cache};
