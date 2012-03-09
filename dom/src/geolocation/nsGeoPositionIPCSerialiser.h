/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla geolocation code.
 *
 * The Initial Developer of the Original Code is Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Mike Kristoffersen <moz@mikek.dk> (Original Author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef dom_src_geolocation_IPC_serialiser
#define dom_src_geolocation_IPC_serialiser

#include "IPC/IPCMessageUtils.h"
#include "nsGeoPosition.h"
#include "nsIDOMGeoPosition.h"

typedef nsGeoPositionCoords       *GeoPositionCoords;
typedef nsIDOMGeoPosition         *GeoPosition;

namespace IPC {

template <>
struct ParamTraits<GeoPositionCoords>
{
  typedef GeoPositionCoords paramType;

  // Function to serialize a geoposition
  static void Write(Message *aMsg, const paramType& aParam)
  {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);
    // If it is a null object, then we are done
    if (isNull) return;

    double coordData;

    aParam->GetLatitude(&coordData);
    WriteParam(aMsg, coordData);

    aParam->GetLongitude(&coordData);
    WriteParam(aMsg, coordData);

    aParam->GetAltitude(&coordData);
    WriteParam(aMsg, coordData);

    aParam->GetAccuracy(&coordData);
    WriteParam(aMsg, coordData);

    aParam->GetAltitudeAccuracy(&coordData);
    WriteParam(aMsg, coordData);

    aParam->GetHeading(&coordData);
    WriteParam(aMsg, coordData);

    aParam->GetSpeed(&coordData);
    WriteParam(aMsg, coordData);
  }

  // Function to de-serialize a geoposition
  static bool Read(const Message* aMsg, void **aIter, paramType* aResult)
  {
    // Check if it is the null pointer we have transfered
    bool isNull;
    if (!ReadParam(aMsg, aIter, &isNull)) return false;

    if (isNull) {
      *aResult = 0;
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
    if (!(   ReadParam(aMsg, aIter, &latitude         )
          && ReadParam(aMsg, aIter, &longitude        )
          && ReadParam(aMsg, aIter, &altitude         )
          && ReadParam(aMsg, aIter, &accuracy         )
          && ReadParam(aMsg, aIter, &altitudeAccuracy )
          && ReadParam(aMsg, aIter, &heading          )
          && ReadParam(aMsg, aIter, &speed            ))) return false;

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
struct ParamTraits<GeoPosition>
{
  typedef GeoPosition paramType;

  // Function to serialize a geoposition
  static void Write(Message *aMsg, const paramType& aParam)
  {
    bool isNull = !aParam;
    WriteParam(aMsg, isNull);
    // If it is a null object, then we are done
    if (isNull) return;

    DOMTimeStamp timeStamp;
    aParam->GetTimestamp(&timeStamp);
    WriteParam(aMsg, timeStamp);

    nsCOMPtr<nsIDOMGeoPositionCoords> coords;
    aParam->GetCoords(getter_AddRefs(coords));
    GeoPositionCoords simpleCoords = static_cast<GeoPositionCoords>(coords.get());
    WriteParam(aMsg, simpleCoords);
  }

  // Function to de-serialize a geoposition
  static bool Read(const Message* aMsg, void **aIter, paramType* aResult)
  {
    // Check if it is the null pointer we have transfered
    bool isNull;
    if (!ReadParam(aMsg, aIter, &isNull)) return false;

    if (isNull) {
      *aResult = 0;
      return true;
    }

    DOMTimeStamp timeStamp;
    GeoPositionCoords coords = nsnull;

    // It's not important to us where it fails, but rather if it fails
    if (!(   ReadParam(aMsg, aIter, &timeStamp)
          && ReadParam(aMsg, aIter, &coords   ))) {
          // note it is fine to do "delete nsnull" in case coords hasn't
          // been allocated
          delete coords;
          return false;
      }

    *aResult = new nsGeoPosition(coords, timeStamp);

    return true;
  };

};

}

#endif
