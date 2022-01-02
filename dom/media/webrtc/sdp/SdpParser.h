/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef _SDPPARSER_H_
#define _SDPPARSER_H_

#include <vector>
#include <string>
#include "sdp/Sdp.h"
#include "sdp/SdpLog.h"

#include "mozilla/Telemetry.h"

namespace mozilla {

class SdpParser {
 public:
  SdpParser() = default;
  virtual ~SdpParser() = default;

  class Results {
   public:
    typedef std::pair<size_t, std::string> Anomaly;
    typedef std::vector<Anomaly> AnomalyVec;
    virtual ~Results() = default;
    UniquePtr<mozilla::Sdp>& Sdp() { return mSdp; }
    AnomalyVec& Errors() { return mErrors; }
    AnomalyVec& Warnings() { return mWarnings; }
    virtual const std::string& ParserName() const = 0;
    bool Ok() const { return mErrors.empty(); }

   protected:
    UniquePtr<mozilla::Sdp> mSdp;
    AnomalyVec mErrors;
    AnomalyVec mWarnings;
  };

  // The name of the parser implementation
  virtual const std::string& Name() const = 0;

  /**
   * This parses the provided text into an SDP object.
   * This returns a nullptr-valued pointer if things go poorly.
   */
  virtual UniquePtr<SdpParser::Results> Parse(const std::string& aText) = 0;

  class InternalResults : public Results {
   public:
    explicit InternalResults(const std::string& aParserName)
        : mParserName(aParserName) {}
    virtual ~InternalResults() = default;

    void SetSdp(UniquePtr<mozilla::Sdp>&& aSdp) { mSdp = std::move(aSdp); }

    void AddParseError(size_t line, const std::string& message) {
      MOZ_LOG(SdpLog, LogLevel::Error,
              ("%s: parser error %s, at line %zu", mParserName.c_str(),
               message.c_str(), line));
      mErrors.push_back(std::make_pair(line, message));
    }

    void AddParseWarning(size_t line, const std::string& message) {
      MOZ_LOG(SdpLog, LogLevel::Warning,
              ("%s: parser warning %s, at line %zu", mParserName.c_str(),
               message.c_str(), line));
      mWarnings.push_back(std::make_pair(line, message));
    }

    const std::string& ParserName() const override { return mParserName; }

   private:
    const std::string mParserName;
  };
};

}  // namespace mozilla

#endif
