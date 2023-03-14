/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { AppConstants } from "resource://gre/modules/AppConstants.sys.mjs";

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  AttributionIOUtils: "resource:///modules/AttributionCode.sys.mjs",
});

let gReadZoneIdPromise = null;
let gTelemetryPromise = null;

export var ProvenanceData = {
  /**
   * Clears cached code/Promises. For testing only.
   */
  _clearCache() {
    if (Services.env.exists("XPCSHELL_TEST_PROFILE_DIR")) {
      gReadZoneIdPromise = null;
      gTelemetryPromise = null;
    }
  },

  /**
   * Returns an nsIFile for the file containing the zone identifier-based
   * provenance data. This currently only exists on Windows. On other platforms,
   * this will return null.
   */
  get zoneIdProvenanceFile() {
    if (AppConstants.platform == "win") {
      let file = Services.dirsvc.get("GreD", Ci.nsIFile);
      file.append("zoneIdProvenanceData");
      return file;
    }
    return null;
  },

  /**
   * Loads the provenance data that the installer copied (along with some
   * metadata) from the Firefox installer used to create the current
   * installation.
   *
   * If it doesn't already exist, creates a global Promise that loads the data
   * from the file and caches it. Subsequent calls get the same Promise. There
   * is no support for re-reading the file from the disk because there is no
   * good reason that the contents of the file should change.
   *
   * Only expected contents will be pulled out of the file. This does not
   * extract arbitrary, unexpected data. Data will be validated, to the extent
   * possible. Most possible data returned has a potential key indicating that
   * we read an unexpected value out of the file.
   *
   * @returns `null` on unsupported OSs. Otherwise, an object with these
   *          possible keys:
   *    `readProvenanceError`
   *      Will be present if there was a problem when Firefox tried to read the
   *      file that ought to have been written by the installer. Possible values
   *      are:
   *        `noSuchFile`, `readError`, `parseError`
   *      If this key is present, no other keys will be present.
   *    `fileSystem`
   *      What filesystem the installer was on. If available, this will be a
   *      string returned from `GetVolumeInformationByHandleW` via its
   *      `lpFileSystemNameBuffer` parameter. Possible values are:
   *        `NTFS`, `FAT32`, `other`, `missing`, `readIniError`
   *      `other` will be used if the value read does not match one of the
   *      expected file systems.
   *      `missing` will be used if the file didn't contain `fileSystem` or
   *      `readFsError` information.
   *      `readIniError` will be used if an error is encountered when reading
   *      the key from the file in the installation directory.
   *    `readFsError`
   *      The reason why the installer file system could not be determined. Will
   *      be present if `readProvenanceError` and `fileSystem` are not. Possible
   *      values are:
   *        `openFile`, `getVolInfo`, `fsUnterminated`, `getBufferSize`,
   *        `convertString`, `unexpected`, `readIniError`
   *      `unexpected` will be used if the value read from the file didn't match
   *      any of the expected values, for some reason.
   *      `missing` will be used if the file didn't contain `fileSystem` or
   *      `readFsError` information.
   *      `readIniError` will be used if an error is encountered when reading
   *      the key from the file in the installation directory.
   *    `readFsErrorCode`
   *      An integer returned by `GetLastError()` indicating, in more detail,
   *      why we failed to obtain the file system. This key may exist if
   *      `readFsError` exists.
   *    `readZoneIdError`
   *      The reason why the installer was unable to read its zone identifier
   *      ADS. Possible values are:
   *        `openFile`, `readFile`, `unexpected`, `readIniError`
   *      `unexpected` will be used if the value read from the file didn't match
   *      any of the expected values, for some reason.
   *      `readIniError` will be used if an error is encountered when reading
   *      the key from the file in the installation directory.
   *    `readZoneIdErrorCode`
   *      An integer returned by `GetLastError()` indicating, in more detail,
   *      why we failed to read the zone identifier ADS. This key may exist if
   *      `readZoneIdError` exists.
   *    `zoneIdFileSize`
   *      This key should exist if Firefox successfully read the file in the
   *      installation directory and the installer successfully opened the ADS.
   *      If the installer failed to get the size of the ADS prior to reading
   *      it, this will be `unknown`. If the installer was able to get the ADS
   *      size, this will be an integer describing how many bytes long it was.
   *      If this value in installation directory's file isn't `unknown` or an
   *      integer, this will be `unexpected`. If an error is encountered when
   *      reading the key from the file in the installation directory, this will
   *      be `readIniError`.
   *    `zoneIdBufferLargeEnough`
   *      This key should exist if Firefox successfully read the file in the
   *      installation directory and the installer successfully opened the ADS.
   *      Indicates whether the zone identifier ADS size was bigger than the
   *      maximum size that the installer will read from it. If we failed to
   *      determine the ADS size, this will be `unknown`. If the installation
   *      directory's file contains an invalid value, this will be `unexpected`.
   *      If an error is encountered when reading the key from the file in the
   *      installation directory, this will be `readIniError`.
   *      Otherwise, this will be a boolean indicating whether or not the buffer
   *      was large enough to fit the ADS data into.
   *    `zoneIdTruncated`
   *      This key should exist if Firefox successfully read the file in the
   *      installation directory and the installer successfully read the ADS.
   *      Indicates whether or not we read through the end of the ADS data when
   *      we copied it. If the installer failed to determine this, this value
   *      will be `unknown`. If the installation directory's file contains an
   *      invalid value, this will be `unexpected`. If an error is encountered
   *      when reading the key from the file in the installation directory, this
   *      will be `readIniError`. Otherwise, this will be a boolean value
   *      indicating whether or not the data that we copied was truncated.
   *    `zoneId`
   *      The Security Zone that the Zone Identifier data indicates that
   *      installer was downloaded from. See this documentation:
   *      https://learn.microsoft.com/en-us/previous-versions/windows/internet-explorer/ie-developer/platform-apis/ms537183(v=vs.85)
   *      This key will be present if `readProvenanceError` and
   *      `readZoneIdError` are not. It will either be a valid zone ID (an
   *      integer between 0 and 4, inclusive), or else it will be `unexpected`,
   *      `missing`, or `readIniError`.
   *    `referrerUrl`
   *      The URL of the download referrer. This key will be present if
   *      `readProvenanceError` and `readZoneIdError` are not. It will either be
   *      a `URL` object, or else it will be `unexpected`, `missing`, or
   *      `readIniError`.
   *    `referrerUrlIsMozilla`
   *      This key will be present if `ReferrerUrl` is a `URL` object. It will
   *      be `true`` if the download referrer appears to be a Mozilla URL.
   *      Otherwise it will be `false`.
   *    `hostUrl`
   *      The URL of the download source. This key will be present if
   *      `readProvenanceError` and `readZoneIdError` are not. It will either be
   *      a `URL` object, or else it will be `unexpected`, `missing`, or
   *      `readIniError`
   *    `hostUrlIsMozilla`
   *      This key will be present if `HostUrl` is a `URL` object. It will
   *      be `true`` if the download source appears to be a Mozilla URL.
   *      Otherwise it will be `false`.
   */
  async readZoneIdProvenanceFile() {
    if (gReadZoneIdPromise) {
      return gReadZoneIdPromise;
    }
    gReadZoneIdPromise = (async () => {
      let file = this.zoneIdProvenanceFile;
      if (!file) {
        return null;
      }
      let iniData;
      try {
        iniData = await lazy.AttributionIOUtils.readUTF8(file.path);
      } catch (ex) {
        if (DOMException.isInstance(ex) && ex.name == "NotFoundError") {
          return { readProvenanceError: "noSuchFile" };
        }
        return { readProvenanceError: "readError" };
      }

      let ini;
      try {
        // We would rather use asynchronous I/O, so we are going to read the
        // file with IOUtils and then pass the result into the INI parser
        // rather than just giving the INI parser factory the file.
        ini = Cc["@mozilla.org/xpcom/ini-parser-factory;1"]
          .getService(Ci.nsIINIParserFactory)
          .createINIParser(null);
        ini.initFromString(iniData);
      } catch (ex) {
        return { readProvenanceError: "parseError" };
      }

      const unexpectedValueError = "unexpected";
      const missingKeyError = "missing";
      const readIniError = "readIniError";
      const possibleIniErrors = [missingKeyError, readIniError];

      // Format is {"IniSectionName": {"IniKeyName": "IniValue"}}
      let iniValues = {
        Mozilla: {
          fileSystem: null,
          readFsError: null,
          readFsErrorCode: null,
          readZoneIdError: null,
          readZoneIdErrorCode: null,
          zoneIdFileSize: null,
          zoneIdBufferLargeEnough: null,
          zoneIdTruncated: null,
        },
        ZoneTransfer: {
          ZoneId: null,
          ReferrerUrl: null,
          HostUrl: null,
        },
      };

      // The ini reader interface is a little weird in that if we just try to
      // read a value from a known section/key and the section/key doesn't
      // exist, we just get a generic error rather than an indication that
      // the section/key doesn't exist. To distinguish missing keys from any
      // other potential errors, we are going to enumerate the sections and
      // keys use that to determine if we should try to read from them.
      let existingSections;
      try {
        existingSections = Array.from(ini.getSections());
      } catch (ex) {
        return { readProvenanceError: "parseError" };
      }
      for (const section in iniValues) {
        if (!existingSections.includes(section)) {
          for (const key in iniValues[section]) {
            iniValues[section][key] = missingKeyError;
          }
          continue;
        }

        let existingKeys;
        try {
          existingKeys = Array.from(ini.getKeys(section));
        } catch (ex) {
          for (const key in iniValues[section]) {
            iniValues[section][key] = readIniError;
          }
          continue;
        }

        for (const key in iniValues[section]) {
          if (!existingKeys.includes(key)) {
            iniValues[section][key] = missingKeyError;
            continue;
          }

          let value;
          try {
            value = ini.getString(section, key).trim();
          } catch (ex) {
            value = readIniError;
          }
          iniValues[section][key] = value;
        }
      }

      // This helps with how verbose the validation gets.
      const fileSystem = iniValues.Mozilla.fileSystem;
      const readFsError = iniValues.Mozilla.readFsError;
      const readFsErrorCode = iniValues.Mozilla.readFsErrorCode;
      const readZoneIdError = iniValues.Mozilla.readZoneIdError;
      const readZoneIdErrorCode = iniValues.Mozilla.readZoneIdErrorCode;
      const zoneIdFileSize = iniValues.Mozilla.zoneIdFileSize;
      const zoneIdBufferLargeEnough = iniValues.Mozilla.zoneIdBufferLargeEnough;
      const zoneIdTruncated = iniValues.Mozilla.zoneIdTruncated;
      const zoneId = iniValues.ZoneTransfer.ZoneId;
      const referrerUrl = iniValues.ZoneTransfer.ReferrerUrl;
      const hostUrl = iniValues.ZoneTransfer.HostUrl;

      let returnObject = {};

      // readFsError, readFsErrorCode, fileSystem
      const validReadFsErrors = [
        "openFile",
        "getVolInfo",
        "fsUnterminated",
        "getBufferSize",
        "convertString",
      ];
      // These must be upper case
      const validFileSystemValues = ["NTFS", "FAT32"];
      if (fileSystem == missingKeyError && readFsError != missingKeyError) {
        if (
          possibleIniErrors.includes(readFsError) ||
          validReadFsErrors.includes(readFsError)
        ) {
          returnObject.readFsError = readFsError;
        } else {
          returnObject.readFsError = unexpectedValueError;
        }
        if (readFsErrorCode != missingKeyError) {
          let code = parseInt(readFsErrorCode, 10);
          if (!isNaN(code)) {
            returnObject.readFsErrorCode = code;
          }
        }
      } else if (possibleIniErrors.includes(fileSystem)) {
        returnObject.fileSystem = fileSystem;
      } else if (validFileSystemValues.includes(fileSystem.toUpperCase())) {
        returnObject.fileSystem = fileSystem.toUpperCase();
      } else {
        returnObject.fileSystem = "other";
      }

      // zoneIdFileSize
      if (zoneIdFileSize == missingKeyError) {
        // We don't include this one if it's missing.
      } else if (
        zoneIdFileSize == readIniError ||
        zoneIdFileSize == "unknown"
      ) {
        returnObject.zoneIdFileSize = zoneIdFileSize;
      } else {
        let size = parseInt(zoneIdFileSize, 10);
        if (isNaN(size)) {
          returnObject.zoneIdFileSize = unexpectedValueError;
        } else {
          returnObject.zoneIdFileSize = size;
        }
      }

      // zoneIdBufferLargeEnough
      if (zoneIdBufferLargeEnough == missingKeyError) {
        // We don't include this one if it's missing.
      } else if (
        zoneIdBufferLargeEnough == readIniError ||
        zoneIdBufferLargeEnough == "unknown"
      ) {
        returnObject.zoneIdBufferLargeEnough = zoneIdBufferLargeEnough;
      } else if (zoneIdBufferLargeEnough.toLowerCase() == "true") {
        returnObject.zoneIdBufferLargeEnough = true;
      } else if (zoneIdBufferLargeEnough.toLowerCase() == "false") {
        returnObject.zoneIdBufferLargeEnough = false;
      } else {
        returnObject.zoneIdBufferLargeEnough = unexpectedValueError;
      }

      // zoneIdTruncated
      if (zoneIdTruncated == missingKeyError) {
        // We don't include this one if it's missing.
      } else if (
        zoneIdTruncated == readIniError ||
        zoneIdTruncated == "unknown"
      ) {
        returnObject.zoneIdTruncated = zoneIdTruncated;
      } else if (zoneIdTruncated.toLowerCase() == "true") {
        returnObject.zoneIdTruncated = true;
      } else if (zoneIdTruncated.toLowerCase() == "false") {
        returnObject.zoneIdTruncated = false;
      } else {
        returnObject.zoneIdTruncated = unexpectedValueError;
      }

      // readZoneIdError, readZoneIdErrorCode, zoneId, referrerUrl, hostUrl,
      // referrerUrlIsMozilla, hostUrlIsMozilla
      const validReadZoneIdErrors = ["openFile", "readFile"];
      if (
        readZoneIdError != missingKeyError &&
        zoneId == missingKeyError &&
        referrerUrl == missingKeyError &&
        hostUrl == missingKeyError
      ) {
        if (
          possibleIniErrors.includes(readZoneIdError) ||
          validReadZoneIdErrors.includes(readZoneIdError)
        ) {
          returnObject.readZoneIdError = readZoneIdError;
        } else {
          returnObject.readZoneIdError = unexpectedValueError;
        }
        if (readZoneIdErrorCode != missingKeyError) {
          let code = parseInt(readZoneIdErrorCode, 10);
          if (!isNaN(code)) {
            returnObject.readZoneIdErrorCode = code;
          }
        }
      } else {
        if (possibleIniErrors.includes(zoneId)) {
          returnObject.zoneId = zoneId;
        } else {
          let id = parseInt(zoneId, 10);
          if (isNaN(id) || id < 0 || id > 4) {
            returnObject.zoneId = unexpectedValueError;
          } else {
            returnObject.zoneId = id;
          }
        }

        let isMozillaURL = url => {
          const mozillaDomains = ["mozilla.com", "mozilla.net", "mozilla.org"];
          for (const domain of mozillaDomains) {
            if (url.hostname == domain) {
              return true;
            }
            if (url.hostname.endsWith("." + domain)) {
              return true;
            }
          }
          return false;
        };

        if (possibleIniErrors.includes(referrerUrl)) {
          returnObject.referrerUrl = referrerUrl;
        } else {
          try {
            returnObject.referrerUrl = new URL(referrerUrl);
          } catch (ex) {
            returnObject.referrerUrl = unexpectedValueError;
          }
          if (URL.isInstance(returnObject.referrerUrl)) {
            returnObject.referrerUrlIsMozilla = isMozillaURL(
              returnObject.referrerUrl
            );
          }
        }

        if (possibleIniErrors.includes(hostUrl)) {
          returnObject.hostUrl = hostUrl;
        } else {
          try {
            returnObject.hostUrl = new URL(hostUrl);
          } catch (ex) {
            returnObject.hostUrl = unexpectedValueError;
          }
          if (URL.isInstance(returnObject.hostUrl)) {
            returnObject.hostUrlIsMozilla = isMozillaURL(returnObject.hostUrl);
          }
        }
      }

      return returnObject;
    })();
    return gReadZoneIdPromise;
  },

  /**
   * Only submits telemetry once, no matter how many times it is called.
   * Has no effect on OSs where provenance data is not supported.
   *
   * @returns An object indicating the values submitted. Keys may not match the
   *          Scalar names since the returned object is intended to be suitable
   *          for use as a Telemetry Event's `extra` object, which has shorter
   *          limits for extra key names than the limits for Scalar names.
   *          Values will be converted to strings since Telemetry Event's
   *          `extra` objects must have string values.
   *          On platforms that do not support provenance data, this will always
   *          return an empty object.
   */
  async submitProvenanceTelemetry() {
    if (gTelemetryPromise) {
      return gTelemetryPromise;
    }
    gTelemetryPromise = (async () => {
      const errorValue = "error";

      let extra = {};

      let provenance = await this.readZoneIdProvenanceFile();
      if (!provenance) {
        return extra;
      }

      let setTelemetry = (scalarName, extraKey, value) => {
        Services.telemetry.scalarSet(scalarName, value);
        extra[extraKey] = value.toString();
      };

      setTelemetry(
        "attribution.provenance.data_exists",
        "data_exists",
        !provenance.readProvenanceError
      );
      if (provenance.readProvenanceError) {
        return extra;
      }

      setTelemetry(
        "attribution.provenance.file_system",
        "file_system",
        provenance.fileSystem ?? errorValue
      );

      // https://learn.microsoft.com/en-us/windows/win32/debug/system-error-codes--0-499-#ERROR_FILE_NOT_FOUND
      const ERROR_FILE_NOT_FOUND = 2;

      let ads_exists =
        !provenance.readProvenanceError &&
        !(
          provenance.readZoneIdError == "openFile" &&
          provenance.readZoneIdErrorCode == ERROR_FILE_NOT_FOUND
        );
      setTelemetry(
        "attribution.provenance.ads_exists",
        "ads_exists",
        ads_exists
      );
      if (!ads_exists) {
        return extra;
      }

      setTelemetry(
        "attribution.provenance.security_zone",
        "security_zone",
        "zoneId" in provenance ? provenance.zoneId.toString() : errorValue
      );

      let haveReferrerUrl = URL.isInstance(provenance.referrerUrl);
      setTelemetry(
        "attribution.provenance.referrer_url_exists",
        "refer_url_exist",
        haveReferrerUrl
      );
      if (haveReferrerUrl) {
        setTelemetry(
          "attribution.provenance.referrer_url_is_mozilla",
          "refer_url_moz",
          provenance.referrerUrlIsMozilla
        );
      }

      let haveHostUrl = URL.isInstance(provenance.hostUrl);
      setTelemetry(
        "attribution.provenance.host_url_exists",
        "host_url_exist",
        haveHostUrl
      );
      if (haveHostUrl) {
        setTelemetry(
          "attribution.provenance.host_url_is_mozilla",
          "host_url_moz",
          provenance.hostUrlIsMozilla
        );
      }

      return extra;
    })();
    return gTelemetryPromise;
  },
};
