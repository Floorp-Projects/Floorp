/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * vim: sw=4 ts=4 et :
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef PluginPRLibrary_h
#define PluginPRLibrary_h 1

#include "mozilla/PluginLibrary.h"
#include "nsNPAPIPlugin.h"
#include "npfunctions.h"

namespace mozilla {

class PluginPRLibrary : public PluginLibrary
{
public:
    PluginPRLibrary(const char* aFilePath, PRLibrary* aLibrary) :
#if defined(XP_UNIX) && !defined(XP_MACOSX)
        mNP_Initialize(nullptr),
#else
        mNP_Initialize(nullptr),
#endif
        mNP_Shutdown(nullptr),
        mNP_GetMIMEDescription(nullptr),
#if defined(XP_UNIX) && !defined(XP_MACOSX)
        mNP_GetValue(nullptr),
#endif
#if defined(XP_WIN) || defined(XP_MACOSX)
        mNP_GetEntryPoints(nullptr),
#endif
        mNPP_New(nullptr),
        mNPP_ClearSiteData(nullptr),
        mNPP_GetSitesWithData(nullptr),
        mLibrary(aLibrary),
        mFilePath(aFilePath)
    {
        NS_ASSERTION(mLibrary, "need non-null lib");
        // addref here??
    }

    virtual ~PluginPRLibrary()
    {
        // unref here??
    }

    virtual void SetPlugin(nsNPAPIPlugin*) override { }

    virtual bool HasRequiredFunctions() override {
        mNP_Initialize = (NP_InitializeFunc)
            PR_FindFunctionSymbol(mLibrary, "NP_Initialize");
        if (!mNP_Initialize)
            return false;

        mNP_Shutdown = (NP_ShutdownFunc)
            PR_FindFunctionSymbol(mLibrary, "NP_Shutdown");
        if (!mNP_Shutdown)
            return false;

        mNP_GetMIMEDescription = (NP_GetMIMEDescriptionFunc)
            PR_FindFunctionSymbol(mLibrary, "NP_GetMIMEDescription");
#ifndef XP_MACOSX
        if (!mNP_GetMIMEDescription)
            return false;
#endif

#if defined(XP_UNIX) && !defined(XP_MACOSX)
        mNP_GetValue = (NP_GetValueFunc)
            PR_FindFunctionSymbol(mLibrary, "NP_GetValue");
        if (!mNP_GetValue)
            return false;
#endif

#if defined(XP_WIN) || defined(XP_MACOSX)
        mNP_GetEntryPoints = (NP_GetEntryPointsFunc)
            PR_FindFunctionSymbol(mLibrary, "NP_GetEntryPoints");
        if (!mNP_GetEntryPoints)
            return false;
#endif
        return true;
    }

#if defined(XP_UNIX) && !defined(XP_MACOSX) && !defined(MOZ_WIDGET_GONK)
    virtual nsresult NP_Initialize(NPNetscapeFuncs* aNetscapeFuncs,
                                   NPPluginFuncs* aFuncs, NPError* aError) override;
#else
    virtual nsresult NP_Initialize(NPNetscapeFuncs* aNetscapeFuncs,
                                   NPError* aError) override;
#endif

    virtual nsresult NP_Shutdown(NPError* aError) override;
    virtual nsresult NP_GetMIMEDescription(const char** aMimeDesc) override;

    virtual nsresult NP_GetValue(void* aFuture, NPPVariable aVariable,
                                 void* aValue, NPError* aError) override;

#if defined(XP_WIN) || defined(XP_MACOSX)
    virtual nsresult NP_GetEntryPoints(NPPluginFuncs* aFuncs, NPError* aError) override;
#endif

    virtual nsresult NPP_New(NPMIMEType aPluginType, NPP aInstance,
                             uint16_t aMode, int16_t aArgc, char* aArgn[],
                             char* aArgv[], NPSavedData* aSaved,
                             NPError* aError) override;

    virtual nsresult NPP_ClearSiteData(const char* aSite, uint64_t aFlags,
                                       uint64_t aMaxAge, nsCOMPtr<nsIClearSiteDataCallback> callback) override;
    virtual nsresult NPP_GetSitesWithData(nsCOMPtr<nsIGetSitesWithDataCallback> callback) override;

    virtual nsresult AsyncSetWindow(NPP aInstance, NPWindow* aWindow) override;
    virtual nsresult GetImageContainer(NPP aInstance, mozilla::layers::ImageContainer** aContainer) override;
    virtual nsresult GetImageSize(NPP aInstance, nsIntSize* aSize) override;
    virtual bool IsOOP() override { return false; }
#if defined(XP_MACOSX)
    virtual nsresult IsRemoteDrawingCoreAnimation(NPP aInstance, bool* aDrawing) override;
    virtual nsresult ContentsScaleFactorChanged(NPP aInstance, double aContentsScaleFactor) override;
#endif
    virtual nsresult SetBackgroundUnknown(NPP instance) override;
    virtual nsresult BeginUpdateBackground(NPP instance, const nsIntRect&,
                                           DrawTarget** aDrawTarget) override;
    virtual nsresult EndUpdateBackground(NPP instance,
                                         const nsIntRect&) override;
    virtual void DidComposite(NPP aInstance) override { }
    virtual void GetLibraryPath(nsACString& aPath) { aPath.Assign(mFilePath); }
    virtual nsresult GetRunID(uint32_t* aRunID) override { return NS_ERROR_NOT_IMPLEMENTED; }
    virtual void SetHasLocalInstance() override { }
#if defined(XP_WIN)
    virtual nsresult GetScrollCaptureContainer(NPP aInstance, mozilla::layers::ImageContainer** aContainer) override;
#endif
    virtual nsresult HandledWindowedPluginKeyEvent(
                       NPP aInstance,
                       const mozilla::NativeEventData& aNativeKeyData,
                       bool aIsCOnsumed) override;

private:
    NP_InitializeFunc mNP_Initialize;
    NP_ShutdownFunc mNP_Shutdown;
    NP_GetMIMEDescriptionFunc mNP_GetMIMEDescription;
#if defined(XP_UNIX) && !defined(XP_MACOSX)
    NP_GetValueFunc mNP_GetValue;
#endif
#if defined(XP_WIN) || defined(XP_MACOSX)
    NP_GetEntryPointsFunc mNP_GetEntryPoints;
#endif
    NPP_NewProcPtr mNPP_New;
    NPP_ClearSiteDataPtr mNPP_ClearSiteData;
    NPP_GetSitesWithDataPtr mNPP_GetSitesWithData;
    PRLibrary* mLibrary;
    nsCString mFilePath;
};


} // namespace mozilla

#endif  // ifndef PluginPRLibrary_h
