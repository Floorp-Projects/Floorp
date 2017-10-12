/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set ts=2 sw=2 sts=2 et cindent: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef DecoderDoctorLogger_h_
#define DecoderDoctorLogger_h_

#include "DDLoggedTypeTraits.h"
#include "DDLogCategory.h"
#include "DDLogValue.h"
#include "mozilla/Atomics.h"
#include "mozilla/DefineEnum.h"
#include "mozilla/MozPromise.h"
#include "nsString.h"

namespace mozilla {

// Main class used to capture log messages from the media stack, and to
// retrieve processed messages associated with an HTMLMediaElement.
//
// The logging APIs are designed to work as fast as possible (in most cases
// only checking a couple of atomic variables, and not allocating memory), so
// as not to introduce perceptible latency.
// Consider using DDLOG...() macros, and IsDDLoggingEnabled(), to avoid any
// unneeded work when logging is not enabled.
//
// Structural logging messages are used to determine when objects are created
// and destroyed, and to link objects that depend on each other, ultimately
// tying groups of objects and their messages to HTMLMediaElement objects.
//
// A separate thread processes log messages, and can asynchronously retrieve
// processed messages that correspond to a given HTMLMediaElement.
// That thread is also responsible for removing dated messages, so as not to
// take too much memory.
class DecoderDoctorLogger
{
public:
  // Is logging currently enabled? This is tested anyway in all public `Log...`
  // functions, but it may be used to prevent logging-only work in clients.
  static inline bool IsDDLoggingEnabled()
  {
    return MOZ_UNLIKELY(static_cast<LogState>(sLogState) == scEnabled);
  }

  // Shutdown logging. This will prevent more messages to be queued, but the
  // already-queued messages may still get processed.
  static void ShutdownLogging() { sLogState = scShutdown; }

  // Something went horribly wrong, stop all logging and log processing.
  static void Panic(const char* aReason)
  {
    PanicInternal(aReason, /* aDontBlock */ false);
  }

  // Logging functions.
  //
  // All logging functions take:
  // - The object that produces the message, either as a template type (for
  //   which a specialized DDLoggedTypeTraits exists), or a pointer and a type
  //   name (needed for inner classes that cannot specialize DDLoggedTypeTraits.)
  // - A DDLogCategory defining the type of log message; some are used
  //   internally for capture the lifetime and linking of C++ objects, others
  //   are used to split messages into different domains.
  // - A label (string literal).
  // - An optional Variant value, see DDLogValue for the accepted types.
  //
  // The following `EagerLog...` functions always cause their arguments to be
  // pre-evaluated even if logging is disabled, in which case runtime could be
  // wasted. Consider using `DDLOG...` macros instead, or test
  // `IsDDLoggingEnabled()` first.

  template<typename Value>
  static void EagerLogValue(const char* aSubjectTypeName,
                            const void* aSubjectPointer,
                            DDLogCategory aCategory,
                            const char* aLabel,
                            Value&& aValue)
  {
    Log(aSubjectTypeName,
        aSubjectPointer,
        aCategory,
        aLabel,
        DDLogValue{ Forward<Value>(aValue) });
  }

  template<typename Subject, typename Value>
  static void EagerLogValue(const Subject* aSubject,
                            DDLogCategory aCategory,
                            const char* aLabel,
                            Value&& aValue)
  {
    EagerLogValue(DDLoggedTypeTraits<Subject>::Name(),
                  aSubject,
                  aCategory,
                  aLabel,
                  Forward<Value>(aValue));
  }

  // EagerLogValue that can explicitly take strings, as the templated function
  // above confuses Variant when forwarding string literals.
  static void EagerLogValue(const char* aSubjectTypeName,
                            const void* aSubjectPointer,
                            DDLogCategory aCategory,
                            const char* aLabel,
                            const char* aValue)
  {
    Log(aSubjectTypeName,
        aSubjectPointer,
        aCategory,
        aLabel,
        DDLogValue{ aValue });
  }

  template<typename Subject>
  static void EagerLogValue(const Subject* aSubject,
                            DDLogCategory aCategory,
                            const char* aLabel,
                            const char* aValue)
  {
    EagerLogValue(
      DDLoggedTypeTraits<Subject>::Name(), aSubject, aCategory, aLabel, aValue);
  }

  template<typename... Args>
  static void EagerLogPrintf(const char* aSubjectTypeName,
                             const void* aSubjectPointer,
                             DDLogCategory aCategory,
                             const char* aLabel,
                             const char* aFormat,
                             Args&&... aArgs)
  {
    Log(aSubjectTypeName,
        aSubjectPointer,
        aCategory,
        aLabel,
        DDLogValue{
          nsCString{ nsPrintfCString(aFormat, Forward<Args>(aArgs)...) } });
  }

  template<typename Subject, typename... Args>
  static void EagerLogPrintf(const Subject* aSubject,
                             DDLogCategory aCategory,
                             const char* aLabel,
                             const char* aFormat,
                             Args&&... aArgs)
  {
    EagerLogPrintf(DDLoggedTypeTraits<Subject>::Name(),
                   aSubject,
                   aCategory,
                   aLabel,
                   aFormat,
                   Forward<Args>(aArgs)...);
  }

  // Special logging functions. Consider using DecoderDoctorLifeLogger to
  // automatically capture constructions & destructions.

  static void LogConstruction(const char* aSubjectTypeName,
                              const void* aSubjectPointer)
  {
    Log(aSubjectTypeName,
        aSubjectPointer,
        DDLogCategory::_Construction,
        "",
        DDLogValue{ DDNoValue{} });
  }

  static void LogConstructionAndBase(const char* aSubjectTypeName,
                                     const void* aSubjectPointer,
                                     const char* aBaseTypeName,
                                     const void* aBasePointer)
  {
    Log(aSubjectTypeName,
        aSubjectPointer,
        DDLogCategory::_DerivedConstruction,
        "",
        DDLogValue{ DDLogObject{ aBaseTypeName, aBasePointer } });
  }

  template<typename B>
  static void LogConstructionAndBase(const char* aSubjectTypeName,
                                     const void* aSubjectPointer,
                                     const B* aBase)
  {
    Log(aSubjectTypeName,
        aSubjectPointer,
        DDLogCategory::_DerivedConstruction,
        "",
        DDLogValue{ DDLogObject{ DDLoggedTypeTraits<B>::Name(), aBase } });
  }

  template<typename Subject>
  static void LogConstruction(const Subject* aSubject)
  {
    using Traits = DDLoggedTypeTraits<Subject>;
    if (!Traits::HasBase::value) {
      Log(DDLoggedTypeTraits<Subject>::Name(),
          aSubject,
          DDLogCategory::_Construction,
          "",
          DDLogValue{ DDNoValue{} });
    } else {
      Log(DDLoggedTypeTraits<Subject>::Name(),
          aSubject,
          DDLogCategory::_DerivedConstruction,
          "",
          DDLogValue{ DDLogObject{
            DDLoggedTypeTraits<typename Traits::BaseType>::Name(),
            static_cast<const typename Traits::BaseType*>(aSubject) } });
    }
  }

  static void LogDestruction(const char* aSubjectTypeName,
                             const void* aSubjectPointer)
  {
    Log(aSubjectTypeName,
        aSubjectPointer,
        DDLogCategory::_Destruction,
        "",
        DDLogValue{ DDNoValue{} });
  }

  template<typename Subject>
  static void LogDestruction(const Subject* aSubject)
  {
    Log(DDLoggedTypeTraits<Subject>::Name(),
        aSubject,
        DDLogCategory::_Destruction,
        "",
        DDLogValue{ DDNoValue{} });
  }

  template<typename P, typename C>
  static void LinkParentAndChild(const P* aParent,
                                 const char* aLinkName,
                                 const C* aChild)
  {
    if (aChild) {
      Log(DDLoggedTypeTraits<P>::Name(),
          aParent,
          DDLogCategory::_Link,
          aLinkName,
          DDLogValue{ DDLogObject{ DDLoggedTypeTraits<C>::Name(), aChild } });
    }
  }

  template<typename C>
  static void LinkParentAndChild(const char* aParentTypeName,
                                 const void* aParentPointer,
                                 const char* aLinkName,
                                 const C* aChild)
  {
    if (aChild) {
      Log(aParentTypeName,
          aParentPointer,
          DDLogCategory::_Link,
          aLinkName,
          DDLogValue{ DDLogObject{ DDLoggedTypeTraits<C>::Name(), aChild } });
    }
  }

  template<typename P>
  static void LinkParentAndChild(const P* aParent,
                                 const char* aLinkName,
                                 const char* aChildTypeName,
                                 const void* aChildPointer)
  {
    if (aChildPointer) {
      Log(DDLoggedTypeTraits<P>::Name(),
          aParent,
          DDLogCategory::_Link,
          aLinkName,
          DDLogValue{ DDLogObject{ aChildTypeName, aChildPointer } });
    }
  }

  template<typename C>
  static void UnlinkParentAndChild(const char* aParentTypeName,
                                   const void* aParentPointer,
                                   const C* aChild)
  {
    if (aChild) {
      Log(aParentTypeName,
          aParentPointer,
          DDLogCategory::_Unlink,
          "",
          DDLogValue{ DDLogObject{ DDLoggedTypeTraits<C>::Name(), aChild } });
    }
  }

  template<typename P, typename C>
  static void UnlinkParentAndChild(const P* aParent, const C* aChild)
  {
    if (aChild) {
      Log(DDLoggedTypeTraits<P>::Name(),
          aParent,
          DDLogCategory::_Unlink,
          "",
          DDLogValue{ DDLogObject{ DDLoggedTypeTraits<C>::Name(), aChild } });
    }
  }

  // Retrieval functions.

  // Enable logging, if not done already. No effect otherwise.
  static void EnableLogging();

  using LogMessagesPromise =
    MozPromise<nsCString, nsresult, /* IsExclusive = */ true>;

  // Retrieve all messages related to a given HTMLMediaElement object.
  // This call will trigger a processing run (to ensure the most recent data
  // will be returned), and the returned promise will be resolved with all
  // relevant log messages and object lifetimes in a JSON string.
  // The first call will enable logging, until shutdown.
  static RefPtr<LogMessagesPromise> RetrieveMessages(
    const dom::HTMLMediaElement* aMediaElement);

private:
  // If logging is not enabled yet, initiate it, return true.
  // If logging has been shutdown, don't start it, return false.
  // Otherwise return true.
  static bool EnsureLogIsEnabled();

  // Note that this call may block while the state is scEnabling;
  // set aDontBlock to true to avoid blocking, most importantly when the
  // caller is the one doing the enabling, this would cause an endless loop.
  static void PanicInternal(const char* aReason, bool aDontBlock);

  static void Log(const char* aSubjectTypeName,
                  const void* aSubjectPointer,
                  DDLogCategory aCategory,
                  const char* aLabel,
                  DDLogValue&& aValue);

  using LogState = int;
  // Currently disabled, may be enabled on request.
  static constexpr LogState scDisabled = 0;
  // Currently enabled (logging happens), may be shutdown.
  static constexpr LogState scEnabled = 1;
  // Still disabled, but one thread is working on enabling it, nobody else
  // should interfere during this time.
  static constexpr LogState scEnabling = 2;
  // Shutdown, cannot be re-enabled.
  static constexpr LogState scShutdown = 3;
  // Current state.
  static Atomic<LogState> sLogState;

  // If non-null, reason for an abnormal shutdown.
  static const char* sShutdownReason;
};

// Base class to automatically record a class lifetime. Usage:
//   class SomeClass : public DecoderDoctorLifeLogger<SomeClass>
//   {
//     ...
template<typename T>
class DecoderDoctorLifeLogger
{
public:
  DecoderDoctorLifeLogger()
  {
    DecoderDoctorLogger::LogConstruction(static_cast<const T*>(this));
  }
  ~DecoderDoctorLifeLogger()
  {
    DecoderDoctorLogger::LogDestruction(static_cast<const T*>(this));
  }
};

// Macros to help lazily-evaluate arguments, only after we have checked that
// logging is enabled.

// Log a single value; see DDLogValue for allowed types.
#define DDLOG(_category, _label, _arg)                                         \
  do {                                                                         \
    if (DecoderDoctorLogger::IsDDLoggingEnabled()) {                           \
      DecoderDoctorLogger::EagerLogValue(this, _category, _label, _arg);       \
    }                                                                          \
  } while (0)
// Log a single value, with an EXplicit `this`.
#define DDLOGEX(_this, _category, _label, _arg)                                \
  do {                                                                         \
    if (DecoderDoctorLogger::IsDDLoggingEnabled()) {                           \
      DecoderDoctorLogger::EagerLogValue(_this, _category, _label, _arg);      \
    }                                                                          \
  } while (0)
// Log a single value, with EXplicit type name and `this`.
#define DDLOGEX2(_typename, _this, _category, _label, _arg)                    \
  do {                                                                         \
    if (DecoderDoctorLogger::IsDDLoggingEnabled()) {                           \
      DecoderDoctorLogger::EagerLogValue(                                      \
        _typename, _this, _category, _label, _arg);                            \
    }                                                                          \
  } while (0)

#ifdef DEBUG
// Do a printf format check in DEBUG, with the downside that side-effects (from
// evaluating the arguments) may happen twice! Who would do that anyway?
static void inline MOZ_FORMAT_PRINTF(1, 2) DDLOGPRCheck(const char*, ...) {}
#define DDLOGPR_CHECK(_fmt, ...) DDLOGPRCheck(_fmt, __VA_ARGS__)
#else
#define DDLOGPR_CHECK(_fmt, ...)
#endif

// Log a printf'd string. Discouraged, please try using DDLOG instead.
#define DDLOGPR(_category, _label, _format, ...)                               \
  do {                                                                         \
    if (DecoderDoctorLogger::IsDDLoggingEnabled()) {                           \
      DDLOGPR_CHECK(_format, __VA_ARGS__);                                     \
      DecoderDoctorLogger::EagerLogPrintf(                                     \
        this, _category, _label, _format, __VA_ARGS__);                        \
    }                                                                          \
  } while (0)

// Link a child object.
#define DDLINKCHILD(...)                                                       \
  do {                                                                         \
    if (DecoderDoctorLogger::IsDDLoggingEnabled()) {                           \
      DecoderDoctorLogger::LinkParentAndChild(this, __VA_ARGS__);              \
    }                                                                          \
  } while (0)

// Unlink a child object.
#define DDUNLINKCHILD(...)                                                     \
  do {                                                                         \
    if (DecoderDoctorLogger::IsDDLoggingEnabled()) {                           \
      DecoderDoctorLogger::UnlinkParentAndChild(this, __VA_ARGS__);            \
    }                                                                          \
  } while (0)

} // namespace mozilla

#endif // DecoderDoctorLogger_h_
