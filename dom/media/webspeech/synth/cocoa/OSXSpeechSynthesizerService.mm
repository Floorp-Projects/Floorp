/* -*- Mode: Objective-C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 sw=2 et tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsISupports.h"
#include "nsServiceManagerUtils.h"
#include "nsObjCExceptions.h"
#include "nsCocoaUtils.h"
#include "nsThreadUtils.h"
#include "mozilla/dom/nsSynthVoiceRegistry.h"
#include "mozilla/dom/nsSpeechTask.h"
#include "mozilla/Preferences.h"
#include "mozilla/Assertions.h"
#include "OSXSpeechSynthesizerService.h"

#import <Cocoa/Cocoa.h>

using namespace mozilla;

class SpeechTaskCallback final : public nsISpeechTaskCallback
{
public:
  SpeechTaskCallback(nsISpeechTask* aTask, NSSpeechSynthesizer* aSynth)
    : mTask(aTask)
    , mSpeechSynthesizer(aSynth)
  {
    mStartingTime = TimeStamp::Now();
  }

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(SpeechTaskCallback, nsISpeechTaskCallback)

  NS_DECL_NSISPEECHTASKCALLBACK

  void OnWillSpeakWord(uint32_t aIndex);
  void OnError(uint32_t aIndex);
  void OnDidFinishSpeaking();

private:
  virtual ~SpeechTaskCallback()
  {
    [mSpeechSynthesizer release];
  }

  float GetTimeDurationFromStart();

  nsCOMPtr<nsISpeechTask> mTask;
  NSSpeechSynthesizer* mSpeechSynthesizer;
  TimeStamp mStartingTime;
  uint32_t mCurrentIndex;
};

NS_IMPL_CYCLE_COLLECTION(SpeechTaskCallback, mTask);

NS_INTERFACE_MAP_BEGIN_CYCLE_COLLECTION(SpeechTaskCallback)
  NS_INTERFACE_MAP_ENTRY(nsISpeechTaskCallback)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISpeechTaskCallback)
NS_INTERFACE_MAP_END

NS_IMPL_CYCLE_COLLECTING_ADDREF(SpeechTaskCallback)
NS_IMPL_CYCLE_COLLECTING_RELEASE(SpeechTaskCallback)

NS_IMETHODIMP
SpeechTaskCallback::OnCancel()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [mSpeechSynthesizer stopSpeaking];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
SpeechTaskCallback::OnPause()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [mSpeechSynthesizer pauseSpeakingAtBoundary:NSSpeechImmediateBoundary];
  mTask->DispatchPause(GetTimeDurationFromStart(), mCurrentIndex);
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
SpeechTaskCallback::OnResume()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [mSpeechSynthesizer continueSpeaking];
  mTask->DispatchResume(GetTimeDurationFromStart(), mCurrentIndex);
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
SpeechTaskCallback::OnVolumeChanged(float aVolume)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  [mSpeechSynthesizer setObject:[NSNumber numberWithFloat:aVolume]
                    forProperty:NSSpeechVolumeProperty error:nil];
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

float
SpeechTaskCallback::GetTimeDurationFromStart()
{
  TimeDuration duration = TimeStamp::Now() - mStartingTime;
  return duration.ToMilliseconds();
}

void
SpeechTaskCallback::OnWillSpeakWord(uint32_t aIndex)
{
  mCurrentIndex = aIndex;
  mTask->DispatchBoundary(NS_LITERAL_STRING("word"),
                          GetTimeDurationFromStart(), mCurrentIndex);
}

void
SpeechTaskCallback::OnError(uint32_t aIndex)
{
  mTask->DispatchError(GetTimeDurationFromStart(), aIndex);
}

void
SpeechTaskCallback::OnDidFinishSpeaking()
{
  mTask->DispatchEnd(GetTimeDurationFromStart(), mCurrentIndex);
  // no longer needed
  mTask = nullptr;
}

@interface SpeechDelegate : NSObject<NSSpeechSynthesizerDelegate>
{
@private
  SpeechTaskCallback* mCallback;
}

  - (id)initWithCallback:(SpeechTaskCallback*)aCallback;
@end

@implementation SpeechDelegate
- (id)initWithCallback:(SpeechTaskCallback*)aCallback
{
  [super init];
  mCallback = aCallback;
  return self;
}

- (void)speechSynthesizer:(NSSpeechSynthesizer *)aSender
            willSpeakWord:(NSRange)aRange ofString:(NSString*)aString
{
  mCallback->OnWillSpeakWord(aRange.location);
}

- (void)speechSynthesizer:(NSSpeechSynthesizer *)aSender
        didFinishSpeaking:(BOOL)aFinishedSpeaking
{
  mCallback->OnDidFinishSpeaking();
}

- (void)speechSynthesizer:(NSSpeechSynthesizer*)aSender
 didEncounterErrorAtIndex:(NSUInteger)aCharacterIndex
                 ofString:(NSString*)aString
                  message:(NSString*)aMessage
{
  mCallback->OnError(aCharacterIndex);
}
@end

namespace mozilla {
namespace dom {

class RegisterVoicesRunnable final : public nsRunnable
{
public:
  explicit RegisterVoicesRunnable(OSXSpeechSynthesizerService* aSpeechService)
    : mSpeechService(aSpeechService)
  {
  }

  NS_IMETHOD Run();

private:
  ~RegisterVoicesRunnable()
  {
  }

  RefPtr<OSXSpeechSynthesizerService> mSpeechService;
};

NS_IMETHODIMP
RegisterVoicesRunnable::Run()
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  nsresult rv;
  nsCOMPtr<nsISynthVoiceRegistry> registry =
    do_GetService(NS_SYNTHVOICEREGISTRY_CONTRACTID, &rv);
  if (!registry) {
    return rv;
  }

  NSArray* voices = [NSSpeechSynthesizer availableVoices];
  NSString* defaultVoice = [NSSpeechSynthesizer defaultVoice];

  for (NSString* voice in voices) {
    NSDictionary* attr = [NSSpeechSynthesizer attributesForVoice:voice];

    nsAutoString identifier;
    nsCocoaUtils::GetStringForNSString([attr objectForKey:NSVoiceIdentifier],
                                       identifier);

    nsAutoString name;
    nsCocoaUtils::GetStringForNSString([attr objectForKey:NSVoiceName], name);

    nsAutoString locale;
    nsCocoaUtils::GetStringForNSString(
      [attr objectForKey:NSVoiceLocaleIdentifier], locale);
    locale.ReplaceChar('_', '-');

    nsAutoString uri;
    uri.AssignLiteral("urn:moz-tts:osx:");
    uri.Append(identifier);
    rv = registry->AddVoice(mSpeechService, uri, name, locale, true, false);
    if (NS_WARN_IF(NS_FAILED(rv))) {
      continue;
    }

    if ([voice isEqualToString:defaultVoice]) {
      registry->SetDefaultVoice(uri, true);
    }
  }

  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

StaticRefPtr<OSXSpeechSynthesizerService> OSXSpeechSynthesizerService::sSingleton;

NS_INTERFACE_MAP_BEGIN(OSXSpeechSynthesizerService)
  NS_INTERFACE_MAP_ENTRY(nsISpeechService)
  NS_INTERFACE_MAP_ENTRY(nsIObserver)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsISpeechService)
NS_INTERFACE_MAP_END

NS_IMPL_ADDREF(OSXSpeechSynthesizerService)
NS_IMPL_RELEASE(OSXSpeechSynthesizerService)

OSXSpeechSynthesizerService::OSXSpeechSynthesizerService()
  : mInitialized(false)
{
}

OSXSpeechSynthesizerService::~OSXSpeechSynthesizerService()
{
}

bool
OSXSpeechSynthesizerService::Init()
{
  if (Preferences::GetBool("media.webspeech.synth.test") ||
      !Preferences::GetBool("media.webspeech.synth.enabled")) {
    // When test is enabled, we shouldn't add OS backend (Bug 1160844)
    return false;
  }

  // Get all the voices and register in the SynthVoiceRegistry
  nsCOMPtr<nsIRunnable> runnable = new RegisterVoicesRunnable(this);
  NS_DispatchToMainThread(runnable);

  mInitialized = true;
  return true;
}

NS_IMETHODIMP
OSXSpeechSynthesizerService::Speak(const nsAString& aText,
                                   const nsAString& aUri,
                                   float aVolume,
                                   float aRate,
                                   float aPitch,
                                   nsISpeechTask* aTask)
{
  NS_OBJC_BEGIN_TRY_ABORT_BLOCK_NSRESULT;

  MOZ_ASSERT(StringBeginsWith(aUri, NS_LITERAL_STRING("urn:moz-tts:osx:")),
             "OSXSpeechSynthesizerService doesn't allow this voice URI");

  NSSpeechSynthesizer* synth = [[NSSpeechSynthesizer alloc] init];
  // strlen("urn:moz-tts:osx:") == 16
  NSString* identifier = nsCocoaUtils::ToNSString(Substring(aUri, 16));
  [synth setVoice:identifier];

  // default rate is 180-220
  [synth setObject:[NSNumber numberWithInt:aRate * 200]
         forProperty:NSSpeechRateProperty error:nil];
  // volume allows 0.0-1.0
  [synth setObject:[NSNumber numberWithFloat:aVolume]
         forProperty:NSSpeechVolumeProperty error:nil];
  // Use default pitch value to calculate this
  NSNumber* defaultPitch =
    [synth objectForProperty:NSSpeechPitchBaseProperty error:nil];
  if (defaultPitch) {
    int newPitch = [defaultPitch intValue] * (aPitch / 2 + 0.5);
    [synth setObject:[NSNumber numberWithInt:newPitch]
           forProperty:NSSpeechPitchBaseProperty error:nil];
  }

  RefPtr<SpeechTaskCallback> callback = new SpeechTaskCallback(aTask, synth);
  nsresult rv = aTask->Setup(callback, 0, 0, 0);
  NS_ENSURE_SUCCESS(rv, rv);

  SpeechDelegate* delegate = [[SpeechDelegate alloc] initWithCallback:callback];
  [synth setDelegate:delegate];
  [delegate release ];

  NSString* text = nsCocoaUtils::ToNSString(aText);
  BOOL success = [synth startSpeakingString:text];
  NS_ENSURE_TRUE(success, NS_ERROR_FAILURE);

  aTask->DispatchStart();
  return NS_OK;

  NS_OBJC_END_TRY_ABORT_BLOCK_NSRESULT;
}

NS_IMETHODIMP
OSXSpeechSynthesizerService::GetServiceType(SpeechServiceType* aServiceType)
{
  *aServiceType = nsISpeechService::SERVICETYPE_INDIRECT_AUDIO;
  return NS_OK;
}

NS_IMETHODIMP
OSXSpeechSynthesizerService::Observe(nsISupports* aSubject, const char* aTopic,
                                     const char16_t* aData)
{
  if (!strcmp(aTopic, "profile-after-change")) {
    Init();
  }
  return NS_OK;
}

OSXSpeechSynthesizerService*
OSXSpeechSynthesizerService::GetInstance()
{
  MOZ_ASSERT(NS_IsMainThread());
  if (XRE_GetProcessType() != GeckoProcessType_Default) {
    return nullptr;
  }

  if (!sSingleton) {
    sSingleton = new OSXSpeechSynthesizerService();
  }
  return sSingleton;
}

already_AddRefed<OSXSpeechSynthesizerService>
OSXSpeechSynthesizerService::GetInstanceForService()
{
  RefPtr<OSXSpeechSynthesizerService> speechService = GetInstance();
  return speechService.forget();
}

void
OSXSpeechSynthesizerService::Shutdown()
{
  if (!sSingleton) {
    return;
  }
  sSingleton = nullptr;
}

} // namespace dom
} // namespace mozilla
