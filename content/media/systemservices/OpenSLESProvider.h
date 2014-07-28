/* -*- Mode: C++; tab-width: 50; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _OPENSLESPROVIDER_H_
#define _OPENSLESPROVIDER_H_

#include <SLES/OpenSLES.h>
#include <mozilla/Types.h>

#ifdef __cplusplus
extern "C" {
#endif
extern MOZ_EXPORT
int mozilla_get_sles_engine(SLObjectItf * aObjectm,
                            SLuint32 aOptionCount,
                            const SLEngineOption *aOptions);
extern MOZ_EXPORT
void mozilla_destroy_sles_engine(SLObjectItf * aObjectm);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
#include "mozilla/Mutex.h"

extern PRLogModuleInfo *gOpenSLESProviderLog;

namespace mozilla {

class OpenSLESProvider {
public:
    static int Get(SLObjectItf * aObjectm,
                   SLuint32 aOptionCount,
                   const SLEngineOption *aOptions);
    static void Destroy(SLObjectItf * aObjectm);
private:
    OpenSLESProvider();
    ~OpenSLESProvider();
    OpenSLESProvider(OpenSLESProvider const&); // NO IMPLEMENTATION
    void operator=(OpenSLESProvider const&);   // NO IMPLEMENTATION
    static OpenSLESProvider& getInstance();
    int GetEngine(SLObjectItf * aObjectm,
                  SLuint32 aOptionCount,
                  const SLEngineOption *aOptions);
    int ConstructEngine(SLObjectItf * aObjectm,
                        SLuint32 aOptionCount,
                        const SLEngineOption *aOptions);
    void DestroyEngine(SLObjectItf * aObjectm);

    // Protect all our internal variables
    mozilla::Mutex mLock;
    SLObjectItf mSLEngine;
    int mSLEngineUsers;
    void *mOpenSLESLib;
};

} //namespace
#endif // cplusplus

#endif /* _OPENSLESPROVIDER_H_ */
