/**
 * This test is used to test whether the decoder doctor would report the error
 * on the notification banner (checking that by observing message) or on the web
 * console (checking that by listening to the test event).
 * Error should be reported after calling `DecoderDoctorDiagnostics::StoreXXX`
 * methods.
 * - StoreFormatDiagnostics() [for checking if type is supported]
 * - StoreDecodeError() [when decode error occurs]
 * - StoreEvent() [for reporting audio sink error]
 */

// Only types being listed here would be allowed to display on a
// notification banner. Otherwise, the error would only be showed on the
// web console.
var gAllowedNotificationTypes =
  "MediaWMFNeeded,MediaFFMpegNotFound,MediaUnsupportedLibavcodec,MediaDecodeError,MediaCannotInitializePulseAudio,";

// Used to check if the mime type in the notification is equal to what we set
// before. This mime type doesn't reflect the real world siutation, i.e. not
// every error listed in this test would happen on this type. An example, ffmpeg
// not found would only happen on H264/AAC media.
const gMimeType = "video/mp4";

add_task(async function setupTestingPref() {
  await SpecialPowers.pushPrefEnv({
    set: [
      ["media.decoder-doctor.testing", true],
      ["media.decoder-doctor.verbose", true],
      ["media.decoder-doctor.notifications-allowed", gAllowedNotificationTypes],
    ],
  });
  // transfer types to lower cases in order to match with `DecoderDoctorReportType`
  gAllowedNotificationTypes = gAllowedNotificationTypes.toLowerCase();
});

add_task(async function testWMFIsNeeded() {
  const tab = await createTab("about:blank");
  await setFormatDiagnosticsReportForMimeType(tab, {
    type: "platform-decoder-not-found",
    decoderDoctorReportId: "mediawmfneeded",
    formats: gMimeType,
  });
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testFFMpegNotFound() {
  const tab = await createTab("about:blank");
  await setFormatDiagnosticsReportForMimeType(tab, {
    type: "platform-decoder-not-found",
    decoderDoctorReportId: "mediaplatformdecodernotfound",
    formats: gMimeType,
  });
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testLibAVCodecUnsupported() {
  const tab = await createTab("about:blank");
  await setFormatDiagnosticsReportForMimeType(tab, {
    type: "unsupported-libavcodec",
    decoderDoctorReportId: "mediaunsupportedlibavcodec",
    formats: gMimeType,
  });
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testCanNotPlayNoDecoder() {
  const tab = await createTab("about:blank");
  await setFormatDiagnosticsReportForMimeType(tab, {
    type: "cannot-play",
    decoderDoctorReportId: "mediacannotplaynodecoders",
    formats: gMimeType,
  });
  BrowserTestUtils.removeTab(tab);
});

add_task(async function testNoDecoder() {
  const tab = await createTab("about:blank");
  await setFormatDiagnosticsReportForMimeType(tab, {
    type: "can-play-but-some-missing-decoders",
    decoderDoctorReportId: "medianodecoders",
    formats: gMimeType,
  });
  BrowserTestUtils.removeTab(tab);
});

const gErrorList = [
  "NS_ERROR_DOM_MEDIA_ABORT_ERR",
  "NS_ERROR_DOM_MEDIA_NOT_ALLOWED_ERR",
  "NS_ERROR_DOM_MEDIA_NOT_SUPPORTED_ERR",
  "NS_ERROR_DOM_MEDIA_DECODE_ERR",
  "NS_ERROR_DOM_MEDIA_FATAL_ERR",
  "NS_ERROR_DOM_MEDIA_METADATA_ERR",
  "NS_ERROR_DOM_MEDIA_OVERFLOW_ERR",
  "NS_ERROR_DOM_MEDIA_MEDIASINK_ERR",
  "NS_ERROR_DOM_MEDIA_DEMUXER_ERR",
  "NS_ERROR_DOM_MEDIA_CDM_ERR",
  "NS_ERROR_DOM_MEDIA_CUBEB_INITIALIZATION_ERR",
];

add_task(async function testDecodeError() {
  const type = "decode-error";
  const decoderDoctorReportId = "mediadecodeerror";
  for (let error of gErrorList) {
    const tab = await createTab("about:blank");
    info(`first to try if the error is not allowed to be reported`);
    // No error is allowed to be reported in the notification banner.
    await SpecialPowers.pushPrefEnv({
      set: [["media.decoder-doctor.decode-errors-allowed", ""]],
    });
    await setDecodeError(tab, {
      type,
      decoderDoctorReportId,
      error,
      shouldReportNotification: false,
    });

    // If the notification type is `MediaDecodeError` and the error type is
    // listed in the pref, then the error would be reported to the
    // notification banner.
    info(`Then to try if the error is allowed to be reported`);
    await SpecialPowers.pushPrefEnv({
      set: [["media.decoder-doctor.decode-errors-allowed", error]],
    });
    await setDecodeError(tab, {
      type,
      decoderDoctorReportId,
      error,
      shouldReportNotification: true,
    });
    BrowserTestUtils.removeTab(tab);
  }
});

add_task(async function testAudioSinkFailedStartup() {
  const tab = await createTab("about:blank");
  await setAudioSinkFailedStartup(tab, {
    type: "cannot-initialize-pulseaudio",
    decoderDoctorReportId: "mediacannotinitializepulseaudio",
    // This error comes with `*`, see `DecoderDoctorDiagnostics::StoreEvent`
    formats: "*",
  });
  BrowserTestUtils.removeTab(tab);
});

/**
 * Following are helper functions
 */
async function createTab(url) {
  let tab = await BrowserTestUtils.openNewForegroundTab(window.gBrowser, url);
  // Create observer in the content process in order to check the decoder
  // doctor's notification that would be sent when an error occurs.
  await SpecialPowers.spawn(tab.linkedBrowser, [], _ => {
    content._notificationName = "decoder-doctor-notification";
    content._obs = {
      observe(subject, topic, data) {
        let { type, decoderDoctorReportId, formats } = JSON.parse(data);
        decoderDoctorReportId = decoderDoctorReportId.toLowerCase();
        info(`received '${type}:${decoderDoctorReportId}:${formats}'`);
        if (!this._resolve) {
          ok(false, "receive unexpected notification?");
        }
        if (
          type == this._type &&
          decoderDoctorReportId == this._decoderDoctorReportId &&
          formats == this._formats
        ) {
          ok(true, `received correct notification`);
          Services.obs.removeObserver(content._obs, content._notificationName);
          this._resolve();
          this._resolve = null;
        }
      },
      // Return a promise that will be resolved once receiving a notification
      // which has equal data with the input parameters.
      waitFor({ type, decoderDoctorReportId, formats }) {
        if (this._resolve) {
          ok(false, "already has a pending promise!");
          return Promise.reject();
        }
        Services.obs.addObserver(content._obs, content._notificationName);
        return new Promise(resolve => {
          info(`waiting for '${type}:${decoderDoctorReportId}:${formats}'`);
          this._resolve = resolve;
          this._type = type;
          this._decoderDoctorReportId = decoderDoctorReportId;
          this._formats = formats;
        });
      },
    };
    content._waitForReport = (params, shouldReportNotification) => {
      const reportToConsolePromise = new Promise(r => {
        content.document.addEventListener(
          "mozreportmediaerror",
          _ => {
            r();
          },
          { once: true }
        );
      });
      const reportToNotificationBannerPromise = shouldReportNotification
        ? content._obs.waitFor(params)
        : Promise.resolve();
      info(
        `waitForConsole=true, waitForNotificationBanner=${shouldReportNotification}`
      );
      return Promise.all([
        reportToConsolePromise,
        reportToNotificationBannerPromise,
      ]);
    };
  });
  return tab;
}

async function setFormatDiagnosticsReportForMimeType(tab, params) {
  const shouldReportNotification = gAllowedNotificationTypes.includes(
    params.decoderDoctorReportId
  );
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [params, shouldReportNotification],
    async (params, shouldReportNotification) => {
      const video = content.document.createElement("video");
      SpecialPowers.wrap(video).setFormatDiagnosticsReportForMimeType(
        params.formats,
        params.decoderDoctorReportId
      );
      await content._waitForReport(params, shouldReportNotification);
    }
  );
  ok(true, `finished check for ${params.decoderDoctorReportId}`);
}

async function setDecodeError(tab, params) {
  info(`start check for ${params.error}`);
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [params],
    async (params, shouldReportNotification) => {
      const video = content.document.createElement("video");
      SpecialPowers.wrap(video).setDecodeError(params.error);
      await content._waitForReport(params, params.shouldReportNotification);
    }
  );
  ok(true, `finished check for ${params.error}`);
}

async function setAudioSinkFailedStartup(tab, params) {
  const shouldReportNotification = gAllowedNotificationTypes.includes(
    params.decoderDoctorReportId
  );
  await SpecialPowers.spawn(
    tab.linkedBrowser,
    [params, shouldReportNotification],
    async (params, shouldReportNotification) => {
      const video = content.document.createElement("video");
      const waitPromise = content._waitForReport(
        params,
        shouldReportNotification
      );
      SpecialPowers.wrap(video).setAudioSinkFailedStartup();
      await waitPromise;
    }
  );
}
