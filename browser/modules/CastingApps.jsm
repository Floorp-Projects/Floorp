// -*- Mode: js; tab-width: 2; indent-tabs-mode: nil; js2-basic-offset: 2; js2-skip-preprocessor-directives: t; -*-
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";
this.EXPORTED_SYMBOLS = ["CastingApps"];

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://gre/modules/SimpleServiceDiscovery.jsm");


var CastingApps = {
  _sendEventToVideo: function (element, data) {
    let event = element.ownerDocument.createEvent("CustomEvent");
    event.initCustomEvent("media-videoCasting", false, true, JSON.stringify(data));
    element.dispatchEvent(event);
  },

  makeURI: function (url, charset, baseURI) {
    return Services.io.newURI(url, charset, baseURI);
  },

  getVideo: function (element) {
    if (!element) {
      return null;
    }

    let extensions = SimpleServiceDiscovery.getSupportedExtensions();
    let types = SimpleServiceDiscovery.getSupportedMimeTypes();

    // Grab the poster attribute from the <video>
    let posterURL = element.poster;

    // First, look to see if the <video> has a src attribute
    let sourceURL = element.src;

    // If empty, try the currentSrc
    if (!sourceURL) {
      sourceURL = element.currentSrc;
    }

    if (sourceURL) {
      // Use the file extension to guess the mime type
      let sourceURI = this.makeURI(sourceURL, null, this.makeURI(element.baseURI));
      if (this.allowableExtension(sourceURI, extensions)) {
        return { element: element, source: sourceURI.spec, poster: posterURL, sourceURI: sourceURI};
      }
    }

    // Next, look to see if there is a <source> child element that meets
    // our needs
    let sourceNodes = element.getElementsByTagName("source");
    for (let sourceNode of sourceNodes) {
      let sourceURI = this.makeURI(sourceNode.src, null, this.makeURI(sourceNode.baseURI));

      // Using the type attribute is our ideal way to guess the mime type. Otherwise,
      // fallback to using the file extension to guess the mime type
      if (this.allowableMimeType(sourceNode.type, types) || this.allowableExtension(sourceURI, extensions)) {
        return { element: element, source: sourceURI.spec, poster: posterURL, sourceURI: sourceURI, type: sourceNode.type };
      }
    }

    return null;
  },

  sendVideoToService: function (videoElement, service) {
    if (!service)
      return;

    let video = this.getVideo(videoElement);
    if (!video) {
      return;
    }

    // Make sure we have a player app for the given service
    let app = SimpleServiceDiscovery.findAppForService(service);
    if (!app)
      return;

    video.title = videoElement.ownerDocument.defaultView.top.document.title;
    if (video.element) {
      // If the video is currently playing on the device, pause it
      if (!video.element.paused) {
        video.element.pause();
      }
    }

    app.stop(() => {
      app.start(started => {
        if (!started) {
          Cu.reportError("CastingApps: Unable to start app");
          return;
        }

        app.remoteMedia(remoteMedia => {
          if (!remoteMedia) {
            Cu.reportError("CastingApps: Failed to create remotemedia");
            return;
          }

          this.session = {
            service: service,
            app: app,
            remoteMedia: remoteMedia,
            data: {
              title: video.title,
              source: video.source,
              poster: video.poster
            },
            videoRef: Cu.getWeakReference(video.element)
          };
        }, this);
      });
    });
  },

  getServicesForVideo: function (videoElement) {
    let video = this.getVideo(videoElement);
    if (!video) {
      return {};
    }

    let filteredServices = SimpleServiceDiscovery.services.filter(service => {
      return this.allowableExtension(video.sourceURI, service.extensions) ||
             this.allowableMimeType(video.type, service.types);
    });

    return filteredServices;
  },

  // RemoteMedia callback API methods
  onRemoteMediaStart: function (remoteMedia) {
    if (!this.session) {
      return;
    }

    remoteMedia.load(this.session.data);

    let video = this.session.videoRef.get();
    if (video) {
      this._sendEventToVideo(video, { active: true });
    }
  },

  onRemoteMediaStop: function (remoteMedia) {
  },

  onRemoteMediaStatus: function (remoteMedia) {
  },

  allowableExtension: function (uri, extensions) {
    return (uri instanceof Ci.nsIURL) && extensions.indexOf(uri.fileExtension) != -1;
  },

  allowableMimeType: function (type, types) {
    return types.indexOf(type) != -1;
  }
};
