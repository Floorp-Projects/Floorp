// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/pref_service.h"

#include "app/l10n_util.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/thread.h"
#include "chrome/common/json_value_serializer.h"
#include "chrome/common/notification_service.h"
#include "grit/generated_resources.h"

namespace {

// A helper function for RegisterLocalized*Pref that creates a Value* based on
// the string value in the locale dll.  Because we control the values in a
// locale dll, this should always return a Value of the appropriate type.
Value* CreateLocaleDefaultValue(Value::ValueType type, int message_id) {
  std::wstring resource_string = l10n_util::GetString(message_id);
  DCHECK(!resource_string.empty());
  switch (type) {
    case Value::TYPE_BOOLEAN: {
      if (L"true" == resource_string)
        return Value::CreateBooleanValue(true);
      if (L"false" == resource_string)
        return Value::CreateBooleanValue(false);
      break;
    }

    case Value::TYPE_INTEGER: {
      return Value::CreateIntegerValue(
          StringToInt(WideToUTF16Hack(resource_string)));
      break;
    }

    case Value::TYPE_REAL: {
      return Value::CreateRealValue(
          StringToDouble(WideToUTF16Hack(resource_string)));
      break;
    }

    case Value::TYPE_STRING: {
      return Value::CreateStringValue(resource_string);
      break;
    }

    default: {
      NOTREACHED() <<
          "list and dictionary types can not have default locale values";
    }
  }
  NOTREACHED();
  return Value::CreateNullValue();
}

}  // namespace

PrefService::PrefService(const FilePath& pref_filename,
                         const base::Thread* backend_thread)
    : persistent_(new DictionaryValue),
      transient_(new DictionaryValue),
      writer_(pref_filename, backend_thread) {
  ReloadPersistentPrefs();
}

PrefService::~PrefService() {
  DCHECK(CalledOnValidThread());

  // Verify that there are no pref observers when we shut down.
  for (PrefObserverMap::iterator it = pref_observers_.begin();
       it != pref_observers_.end(); ++it) {
    NotificationObserverList::Iterator obs_iterator(*(it->second));
    if (obs_iterator.GetNext()) {
      LOG(WARNING) << "pref observer found at shutdown " << it->first;
    }
  }

  STLDeleteContainerPointers(prefs_.begin(), prefs_.end());
  prefs_.clear();
  STLDeleteContainerPairSecondPointers(pref_observers_.begin(),
                                       pref_observers_.end());
  pref_observers_.clear();
}

bool PrefService::ReloadPersistentPrefs() {
  DCHECK(CalledOnValidThread());

  JSONFileValueSerializer serializer(writer_.path());
  scoped_ptr<Value> root(serializer.Deserialize(NULL));
  if (!root.get())
    return false;

  // Preferences should always have a dictionary root.
  if (!root->IsType(Value::TYPE_DICTIONARY))
    return false;

  persistent_.reset(static_cast<DictionaryValue*>(root.release()));
  for (PreferenceSet::iterator it = prefs_.begin();
       it != prefs_.end(); ++it) {
    (*it)->root_pref_ = persistent_.get();
  }

  return true;
}

bool PrefService::SavePersistentPrefs() {
  DCHECK(CalledOnValidThread());

  std::string data;
  if (!SerializePrefData(&data))
    return false;

  writer_.WriteNow(data);
  return true;
}

bool PrefService::ScheduleSavePersistentPrefs() {
  DCHECK(CalledOnValidThread());

  std::string data;
  if (!SerializePrefData(&data))
    return false;

  writer_.ScheduleWrite(data);
  return true;
}

void PrefService::RegisterBooleanPref(const wchar_t* path,
                                      bool default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateBooleanValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterIntegerPref(const wchar_t* path,
                                      int default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateIntegerValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterRealPref(const wchar_t* path,
                                   double default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateRealValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterStringPref(const wchar_t* path,
                                     const std::wstring& default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateStringValue(default_value));
  RegisterPreference(pref);
}

void PrefService::RegisterFilePathPref(const wchar_t* path,
                                       const FilePath& default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateStringValue(default_value.value()));
  RegisterPreference(pref);
}

void PrefService::RegisterListPref(const wchar_t* path) {
  Preference* pref = new Preference(persistent_.get(), path,
      new ListValue);
  RegisterPreference(pref);
}

void PrefService::RegisterDictionaryPref(const wchar_t* path) {
  Preference* pref = new Preference(persistent_.get(), path,
      new DictionaryValue());
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedBooleanPref(const wchar_t* path,
                                               int locale_default_message_id) {
  Preference* pref = new Preference(persistent_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_BOOLEAN, locale_default_message_id));
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedIntegerPref(const wchar_t* path,
                                               int locale_default_message_id) {
  Preference* pref = new Preference(persistent_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_INTEGER, locale_default_message_id));
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedRealPref(const wchar_t* path,
                                            int locale_default_message_id) {
  Preference* pref = new Preference(persistent_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_REAL, locale_default_message_id));
  RegisterPreference(pref);
}

void PrefService::RegisterLocalizedStringPref(const wchar_t* path,
                                              int locale_default_message_id) {
  Preference* pref = new Preference(persistent_.get(), path,
      CreateLocaleDefaultValue(Value::TYPE_STRING, locale_default_message_id));
  RegisterPreference(pref);
}

bool PrefService::IsPrefRegistered(const wchar_t* path) {
  DCHECK(CalledOnValidThread());
  // TODO(tc): We can remove this method and just use FindPreference.
  return FindPreference(path) ? true : false;
}

bool PrefService::GetBoolean(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  bool result = false;
  if (transient_->GetBoolean(path, &result))
    return result;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsBoolean(&result);
  DCHECK(rv);
  return result;
}

int PrefService::GetInteger(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  int result = 0;
  if (transient_->GetInteger(path, &result))
    return result;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsInteger(&result);
  DCHECK(rv);
  return result;
}

double PrefService::GetReal(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  double result = 0.0;
  if (transient_->GetReal(path, &result))
    return result;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsReal(&result);
  DCHECK(rv);
  return result;
}

std::wstring PrefService::GetString(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  std::wstring result;
  if (transient_->GetString(path, &result))
    return result;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return result;
  }
  bool rv = pref->GetValue()->GetAsString(&result);
  DCHECK(rv);
  return result;
}

FilePath PrefService::GetFilePath(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  FilePath::StringType result;
  if (transient_->GetString(path, &result))
    return FilePath(result);

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return FilePath(result);
  }
  bool rv = pref->GetValue()->GetAsString(&result);
  DCHECK(rv);
  return FilePath(result);
}

bool PrefService::HasPrefPath(const wchar_t* path) const {
  Value* value = NULL;
  return (transient_->Get(path, &value) || persistent_->Get(path, &value));
}

const PrefService::Preference* PrefService::FindPreference(
    const wchar_t* pref_name) const {
  DCHECK(CalledOnValidThread());
  Preference p(NULL, pref_name, NULL);
  PreferenceSet::const_iterator it = prefs_.find(&p);
  return it == prefs_.end() ? NULL : *it;
}

const DictionaryValue* PrefService::GetDictionary(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  DictionaryValue* result = NULL;
  if (transient_->GetDictionary(path, &result))
    return result;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return NULL;
  }
  const Value* value = pref->GetValue();
  if (value->GetType() == Value::TYPE_NULL)
    return NULL;
  return static_cast<const DictionaryValue*>(value);
}

const ListValue* PrefService::GetList(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  ListValue* result = NULL;
  if (transient_->GetList(path, &result))
    return result;

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return NULL;
  }
  const Value* value = pref->GetValue();
  if (value->GetType() == Value::TYPE_NULL)
    return NULL;
  return static_cast<const ListValue*>(value);
}

void PrefService::AddPrefObserver(const wchar_t* path,
                                  NotificationObserver* obs) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to add an observer for an unregistered pref: "
        << path;
    return;
  }

  // Get the pref observer list associated with the path.
  NotificationObserverList* observer_list = NULL;
  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end()) {
    observer_list = new NotificationObserverList;
    pref_observers_[path] = observer_list;
  } else {
    observer_list = observer_iterator->second;
  }

  // Verify that this observer doesn't already exist.
  NotificationObserverList::Iterator it(*observer_list);
  NotificationObserver* existing_obs;
  while ((existing_obs = it.GetNext()) != NULL) {
    DCHECK(existing_obs != obs) << path << " observer already registered";
    if (existing_obs == obs)
      return;
  }

  // Ok, safe to add the pref observer.
  observer_list->AddObserver(obs);
}

void PrefService::RemovePrefObserver(const wchar_t* path,
                                     NotificationObserver* obs) {
  DCHECK(CalledOnValidThread());

  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path);
  if (observer_iterator == pref_observers_.end()) {
    return;
  }

  NotificationObserverList* observer_list = observer_iterator->second;
  observer_list->RemoveObserver(obs);
}

void PrefService::RegisterPreference(Preference* pref) {
  DCHECK(CalledOnValidThread());

  if (FindPreference(pref->name().c_str())) {
    NOTREACHED() << "Tried to register duplicate pref " << pref->name();
    delete pref;
    return;
  }
  prefs_.insert(pref);
}

void PrefService::ClearPref(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to clear an unregistered pref: " << path;
    return;
  }

  transient_->Remove(path, NULL);
  Value* value;
  bool has_old_value = persistent_->Get(path, &value);
  persistent_->Remove(path, NULL);

  if (has_old_value)
    FireObservers(path);
}

void PrefService::SetBoolean(const wchar_t* path, bool value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_BOOLEAN) {
    NOTREACHED() << "Wrong type for SetBoolean: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  bool rv = persistent_->SetBoolean(path, value);
  DCHECK(rv);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetInteger(const wchar_t* path, int value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_INTEGER) {
    NOTREACHED() << "Wrong type for SetInteger: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  bool rv = persistent_->SetInteger(path, value);
  DCHECK(rv);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetReal(const wchar_t* path, double value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_REAL) {
    NOTREACHED() << "Wrong type for SetReal: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  bool rv = persistent_->SetReal(path, value);
  DCHECK(rv);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetString(const wchar_t* path, const std::wstring& value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_STRING) {
    NOTREACHED() << "Wrong type for SetString: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  bool rv = persistent_->SetString(path, value);
  DCHECK(rv);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetFilePath(const wchar_t* path, const FilePath& value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_STRING) {
    NOTREACHED() << "Wrong type for SetFilePath: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  bool rv = persistent_->SetString(path, value.value());
  DCHECK(rv);

  FireObserversIfChanged(path, old_value.get());
}

void PrefService::SetInt64(const wchar_t* path, int64_t value) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to write an unregistered pref: " << path;
    return;
  }
  if (pref->type() != Value::TYPE_STRING) {
    NOTREACHED() << "Wrong type for SetInt64: " << path;
    return;
  }

  scoped_ptr<Value> old_value(GetPrefCopy(path));
  bool rv = persistent_->SetString(path, Int64ToWString(value));
  DCHECK(rv);

  FireObserversIfChanged(path, old_value.get());
}

int64_t PrefService::GetInt64(const wchar_t* path) const {
  DCHECK(CalledOnValidThread());

  std::wstring result;
  if (transient_->GetString(path, &result))
    return StringToInt64(WideToUTF16Hack(result));

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to read an unregistered pref: " << path;
    return StringToInt64(WideToUTF16Hack(result));
  }
  bool rv = pref->GetValue()->GetAsString(&result);
  DCHECK(rv);
  return StringToInt64(WideToUTF16Hack(result));
}

void PrefService::RegisterInt64Pref(const wchar_t* path, int64_t default_value) {
  Preference* pref = new Preference(persistent_.get(), path,
      Value::CreateStringValue(Int64ToWString(default_value)));
  RegisterPreference(pref);
}

DictionaryValue* PrefService::GetMutableDictionary(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return NULL;
  }
  if (pref->type() != Value::TYPE_DICTIONARY) {
    NOTREACHED() << "Wrong type for GetMutableDictionary: " << path;
    return NULL;
  }

  DictionaryValue* dict = NULL;
  bool rv = persistent_->GetDictionary(path, &dict);
  if (!rv) {
    dict = new DictionaryValue;
    rv = persistent_->Set(path, dict);
    DCHECK(rv);
  }
  return dict;
}

ListValue* PrefService::GetMutableList(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  if (!pref) {
    NOTREACHED() << "Trying to get an unregistered pref: " << path;
    return NULL;
  }
  if (pref->type() != Value::TYPE_LIST) {
    NOTREACHED() << "Wrong type for GetMutableList: " << path;
    return NULL;
  }

  ListValue* list = NULL;
  bool rv = persistent_->GetList(path, &list);
  if (!rv) {
    list = new ListValue;
    rv = persistent_->Set(path, list);
    DCHECK(rv);
  }
  return list;
}

Value* PrefService::GetPrefCopy(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  const Preference* pref = FindPreference(path);
  DCHECK(pref);
  return pref->GetValue()->DeepCopy();
}

void PrefService::FireObserversIfChanged(const wchar_t* path,
                                         const Value* old_value) {
  Value* new_value = NULL;
  persistent_->Get(path, &new_value);
  if (!old_value->Equals(new_value))
    FireObservers(path);
}

void PrefService::FireObservers(const wchar_t* path) {
  DCHECK(CalledOnValidThread());

  // Convert path to a std::wstring because the Details constructor requires a
  // class.
  std::wstring path_str(path);
  PrefObserverMap::iterator observer_iterator = pref_observers_.find(path_str);
  if (observer_iterator == pref_observers_.end())
    return;

  NotificationObserverList::Iterator it(*(observer_iterator->second));
  NotificationObserver* observer;
  while ((observer = it.GetNext()) != NULL) {
    observer->Observe(NotificationType::PREF_CHANGED,
                      Source<PrefService>(this),
                      Details<std::wstring>(&path_str));
  }
}

bool PrefService::SerializePrefData(std::string* output) const {
  // TODO(tc): Do we want to prune webkit preferences that match the default
  // value?
  JSONStringValueSerializer serializer(output);
  serializer.set_pretty_print(true);
  return serializer.Serialize(*(persistent_.get()));
}

///////////////////////////////////////////////////////////////////////////////
// PrefService::Preference

PrefService::Preference::Preference(DictionaryValue* root_pref,
                                    const wchar_t* name,
                                    Value* default_value)
      : type_(Value::TYPE_NULL),
        name_(name),
        default_value_(default_value),
        root_pref_(root_pref) {
  DCHECK(name);

  if (default_value) {
    type_ = default_value->GetType();
    DCHECK(type_ != Value::TYPE_NULL && type_ != Value::TYPE_BINARY) <<
        "invalid preference type: " << type_;
  }

  // We set the default value of lists and dictionaries to be null so it's
  // easier for callers to check for empty list/dict prefs.
  if (Value::TYPE_LIST == type_ || Value::TYPE_DICTIONARY == type_)
    default_value_.reset(Value::CreateNullValue());
}

const Value* PrefService::Preference::GetValue() const {
  DCHECK(NULL != root_pref_) <<
      "Must register pref before getting its value";

  Value* temp_value = NULL;
  if (root_pref_->Get(name_.c_str(), &temp_value) &&
      temp_value->GetType() == type_) {
    return temp_value;
  }

  // Pref not found, just return the app default.
  return default_value_.get();
}

bool PrefService::Preference::IsDefaultValue() const {
  DCHECK(default_value_.get());
  return default_value_->Equals(GetValue());
}
