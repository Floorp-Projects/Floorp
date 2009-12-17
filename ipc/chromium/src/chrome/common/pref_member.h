// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// A helper class that stays in sync with a preference (bool, int, real, or
// string).  For example:
//
// class MyClass {
//  public:
//   MyClass(PrefService* prefs) {
//     my_string_.Init(prefs::kHomePage, prefs, NULL /* no observer */);
//   }
//  private:
//   StringPrefMember my_string_;
// };
//
// my_string_ should stay in sync with the prefs::kHomePage pref and will
// update if either the pref changes or if my_string_.SetValue is called.
//
// An optional observer can be passed into the Init method which can be used to
// notify MyClass of changes. Note that if you use SetValue(), the observer
// will not be notified.

#ifndef CHROME_COMMON_PREF_MEMBER_H_
#define CHROME_COMMON_PREF_MEMBER_H_

#include <string>

#include "base/basictypes.h"
#include "chrome/common/notification_observer.h"

class PrefService;

namespace subtle {

class PrefMemberBase : public NotificationObserver {
 protected:
  PrefMemberBase();
  virtual ~PrefMemberBase();

  // See PrefMember<> for description.
  void Init(const wchar_t* pref_name, PrefService* prefs,
            NotificationObserver* observer);

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  void VerifyValuePrefName();

  // This methods is used to do the actual sync with pref of the specified type.
  virtual void UpdateValueFromPref() = 0;

  const std::wstring& pref_name() const { return pref_name_; }
  PrefService* prefs() { return prefs_; }

 // Ordered the members to compact the class instance.
 private:
  std::wstring pref_name_;
  NotificationObserver* observer_;
  PrefService* prefs_;

 protected:
  bool is_synced_;
  bool setting_value_;
};

}  // namespace subtle


template <typename ValueType>
class PrefMember : public subtle::PrefMemberBase {
 public:
  // Defer initialization to an Init method so it's easy to make this class be
  // a member variable.
  PrefMember() { }
  virtual ~PrefMember() { }

  // Do the actual initialization of the class.  |observer| may be null if you
  // don't want any notifications of changes.
  void Init(const wchar_t* pref_name, PrefService* prefs,
            NotificationObserver* observer) {
    subtle::PrefMemberBase::Init(pref_name, prefs, observer);
  }

  // Retrieve the value of the member variable.
  ValueType GetValue() {
    VerifyValuePrefName();
    // We lazily fetch the value from the pref service the first time GetValue
    // is called.
    if (!is_synced_) {
      UpdateValueFromPref();
      is_synced_ = true;
    }
    return value_;
  }

  // Provided as a convenience.
  ValueType operator*() {
    return GetValue();
  }

  // Set the value of the member variable.
  void SetValue(const ValueType& value) {
    VerifyValuePrefName();
    setting_value_ = true;
    UpdatePref(value);
    setting_value_ = false;
  }

 protected:
  // This methods is used to do the actual sync with pref of the specified type.
  virtual void UpdatePref(const ValueType& value) = 0;

  // We cache the value of the pref so we don't have to keep walking the pref
  // tree.
  ValueType value_;
};

///////////////////////////////////////////////////////////////////////////////
// Implementations of Boolean, Integer, Real, and String PrefMember below.

class BooleanPrefMember : public PrefMember<bool> {
 public:
  BooleanPrefMember() : PrefMember<bool>() { }
  virtual ~BooleanPrefMember() { }

 protected:
  virtual void UpdateValueFromPref();
  virtual void UpdatePref(const bool& value);

 private:
  DISALLOW_COPY_AND_ASSIGN(BooleanPrefMember);
};

class IntegerPrefMember : public PrefMember<int> {
 public:
  IntegerPrefMember() : PrefMember<int>() { }
  virtual ~IntegerPrefMember() { }

 protected:
  virtual void UpdateValueFromPref();
  virtual void UpdatePref(const int& value);

 private:
  DISALLOW_COPY_AND_ASSIGN(IntegerPrefMember);
};

class RealPrefMember : public PrefMember<double> {
 public:
  RealPrefMember() : PrefMember<double>() { }
  virtual ~RealPrefMember() { }

 protected:
  virtual void UpdateValueFromPref();
  virtual void UpdatePref(const double& value);

 private:
  DISALLOW_COPY_AND_ASSIGN(RealPrefMember);
};

class StringPrefMember : public PrefMember<std::wstring> {
 public:
  StringPrefMember() : PrefMember<std::wstring>() { }
  virtual ~StringPrefMember() { }

 protected:
  virtual void UpdateValueFromPref();
  virtual void UpdatePref(const std::wstring& value);

 private:
  DISALLOW_COPY_AND_ASSIGN(StringPrefMember);
};

#endif  // CHROME_COMMON_PREF_MEMBER_H_
