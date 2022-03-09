/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef dom_src_geolocation_IPC_serialiser
#define dom_src_geolocation_IPC_serialiser

#include "ipc/IPCMessageUtils.h"
#include "mozilla/dom/GeolocationPosition.h"
#include "nsIDOMGeoPosition.h"

namespace IPC {

template <>
struct ParamTraits<nsIDOMGeoPositionCoords*> {
  // Function to serialize a geoposition
  static void Write(MessageWriter* aWriter, nsIDOMGeoPositionCoords* aParam) {
    bool isNull = !aParam;
    WriteParam(aWriter, isNull);
    // If it is a null object, then we are done
    if (isNull) return;

    double coordData;

    aParam->GetLatitude(&coordData);
    WriteParam(aWriter, coordData);

    aParam->GetLongitude(&coordData);
    WriteParam(aWriter, coordData);

    aParam->GetAltitude(&coordData);
    WriteParam(aWriter, coordData);

    aParam->GetAccuracy(&coordData);
    WriteParam(aWriter, coordData);

    aParam->GetAltitudeAccuracy(&coordData);
    WriteParam(aWriter, coordData);

    aParam->GetHeading(&coordData);
    WriteParam(aWriter, coordData);

    aParam->GetSpeed(&coordData);
    WriteParam(aWriter, coordData);
  }

  // Function to de-serialize a geoposition
  static bool Read(MessageReader* aReader,
                   RefPtr<nsIDOMGeoPositionCoords>* aResult) {
    // Check if it is the null pointer we have transfered
    bool isNull;
    if (!ReadParam(aReader, &isNull)) return false;

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    double latitude;
    double longitude;
    double altitude;
    double accuracy;
    double altitudeAccuracy;
    double heading;
    double speed;

    // It's not important to us where it fails, but rather if it fails
    if (!(ReadParam(aReader, &latitude) && ReadParam(aReader, &longitude) &&
          ReadParam(aReader, &altitude) && ReadParam(aReader, &accuracy) &&
          ReadParam(aReader, &altitudeAccuracy) &&
          ReadParam(aReader, &heading) && ReadParam(aReader, &speed)))
      return false;

    // We now have all the data
    *aResult = new nsGeoPositionCoords(latitude,         /* aLat     */
                                       longitude,        /* aLong    */
                                       altitude,         /* aAlt     */
                                       accuracy,         /* aHError  */
                                       altitudeAccuracy, /* aVError  */
                                       heading,          /* aHeading */
                                       speed             /* aSpeed   */
    );
    return true;
  }
};

template <>
struct ParamTraits<nsIDOMGeoPosition*> {
  // Function to serialize a geoposition
  static void Write(MessageWriter* aWriter, nsIDOMGeoPosition* aParam) {
    bool isNull = !aParam;
    WriteParam(aWriter, isNull);
    // If it is a null object, then we are done
    if (isNull) return;

    EpochTimeStamp timeStamp;
    aParam->GetTimestamp(&timeStamp);
    WriteParam(aWriter, timeStamp);

    nsCOMPtr<nsIDOMGeoPositionCoords> coords;
    aParam->GetCoords(getter_AddRefs(coords));
    WriteParam(aWriter, coords);
  }

  // Function to de-serialize a geoposition
  static bool Read(MessageReader* aReader, RefPtr<nsIDOMGeoPosition>* aResult) {
    // Check if it is the null pointer we have transfered
    bool isNull;
    if (!ReadParam(aReader, &isNull)) return false;

    if (isNull) {
      *aResult = nullptr;
      return true;
    }

    EpochTimeStamp timeStamp;
    RefPtr<nsIDOMGeoPositionCoords> coords;

    // It's not important to us where it fails, but rather if it fails
    if (!ReadParam(aReader, &timeStamp) || !ReadParam(aReader, &coords)) {
      return false;
    }

    *aResult = new nsGeoPosition(coords, timeStamp);

    return true;
  };
};

}  // namespace IPC

#endif
