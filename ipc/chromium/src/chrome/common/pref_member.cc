// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_member.h"

#include "base/logging.h"
#include "chrome/common/notification_type.h"
#include "chrome/common/pref_service.h"

namespace subtle {

PrefMemberBase::PrefMemberBase()
    : observer_(NULL),
      prefs_(NULL),
      is_synced_(false),
      setting_value_(false) {
}

PrefMemberBase::~PrefMemberBase() {
  if (!pref_name_.empty())
    prefs_->RemovePrefObserver(pref_name_.c_str(), this);
}


void PrefMemberBase::Init(const wchar_t* pref_name, PrefService* prefs,
                          NotificationObserver* observer) {
  DCHECK(pref_name);
  DCHECK(prefs);
  DCHECK(pref_name_.empty());  // Check that Init is only called once.
  observer_ = observer;
  prefs_ = prefs;
  pref_name_ = pref_name;
  DCHECK(!pref_name_.empty());

  // Add ourself as a pref observer so we can keep our local value in sync.
  prefs_->AddPrefObserver(pref_name, this);
}

void PrefMemberBase::Observe(NotificationType type,
                             const NotificationSource& source,
                             const NotificationDetails& details) {
  DCHECK(!pref_name_.empty());
  DCHECK(NotificationType::PREF_CHANGED == type);
  UpdateValueFromPref();
  is_synced_ = true;
  if (!setting_value_ && observer_)
    observer_->Observe(type, source, details);
}

void PrefMemberBase::VerifyValuePrefName() {
  DCHECK(!pref_name_.empty());
}

}  // namespace subtle

void BooleanPrefMember::UpdateValueFromPref() {
  value_ = prefs()->GetBoolean(pref_name().c_str());
}

void BooleanPrefMember::UpdatePref(const bool& value) {
  prefs()->SetBoolean(pref_name().c_str(), value);
}

void IntegerPrefMember::UpdateValueFromPref() {
  value_ = prefs()->GetInteger(pref_name().c_str());
}

void IntegerPrefMember::UpdatePref(const int& value) {
  prefs()->SetInteger(pref_name().c_str(), value);
}

void RealPrefMember::UpdateValueFromPref() {
  value_ = prefs()->GetReal(pref_name().c_str());
}

void RealPrefMember::UpdatePref(const double& value) {
  prefs()->SetReal(pref_name().c_str(), value);
}

void StringPrefMember::UpdateValueFromPref() {
  value_ = prefs()->GetString(pref_name().c_str());
}

void StringPrefMember::UpdatePref(const std::wstring& value) {
  prefs()->SetString(pref_name().c_str(), value);
}
