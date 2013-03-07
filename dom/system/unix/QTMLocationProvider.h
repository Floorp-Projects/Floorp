/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef QTMLocationProvider_h
#define QTMLocationProvider_h

#include <QGeoPositionInfoSource>
#include "nsGeolocation.h"
#include "nsIGeolocationProvider.h"
#include "nsCOMPtr.h"


using namespace QtMobility;

class QTMLocationProvider : public QObject,
                            public nsIGeolocationProvider
{
    Q_OBJECT

public:
    NS_DECL_ISUPPORTS
    NS_DECL_NSIGEOLOCATIONPROVIDER

    QTMLocationProvider();

public Q_SLOTS:
    // QGeoPositionInfoSource
    void positionUpdated(const QGeoPositionInfo&);

private:
    ~QTMLocationProvider();

    QGeoPositionInfoSource* mLocation;
    nsCOMPtr<nsIGeolocationUpdate> mCallback;
};

#endif /* QTMLocationProvider_h */
