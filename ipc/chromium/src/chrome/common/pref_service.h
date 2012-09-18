// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This provides a way to access the application's current preferences.
// This service has two preference stores, one for "persistent" preferences,
// which get serialized for use in the next session, and one for "transient"
// preferences, which are in effect for only the current session
// (this usually encodes things like command-line switches).
//
// Calling the getter functions in this class basically looks at both the
// persistent and transient stores, where any corresponding value in the
// transient store overrides the one in the persistent store.

#ifndef CHROME_COMMON_PREF_SERVICE_H_
#define CHROME_COMMON_PREF_SERVICE_H_

#include <set>

#include "base/file_path.h"
#include "base/hash_tables.h"
#include "base/non_thread_safe.h"
#include "base/observer_list.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/common/important_file_writer.h"

class NotificationObserver;
class Preference;

namespace base {
class Thread;
}

class PrefService : public NonThreadSafe {
 public:

  // A helper class to store all the information associated with a preference.
  class Preference {
   public:

    // The type of the preference is determined by the type of |default_value|.
    // Therefore, the type needs to be a boolean, integer, real, string,
    // dictionary (a branch), or list.  You shouldn't need to construct this on
    // your own, use the PrefService::Register*Pref methods instead.
    // |default_value| will be owned by the Preference object.
    Preference(DictionaryValue* root_pref,
               const wchar_t* name,
               Value* default_value);
    ~Preference() {}

    Value::ValueType type() const { return type_; }

    // Returns the name of the Preference (i.e., the key, e.g.,
    // browser.window_placement).
    const std::wstring name() const { return name_; }

    // Returns the value of the Preference.  If there is no user specified
    // value, it returns the default value.
    const Value* GetValue() const;

    // Returns true if the current value matches the default value.
    bool IsDefaultValue() const;

   private:
    friend class PrefService;

    Value::ValueType type_;
    std::wstring name_;
    scoped_ptr<Value> default_value_;

    // A reference to the pref service's persistent prefs.
    DictionaryValue* root_pref_;

    DISALLOW_COPY_AND_ASSIGN(Preference);
  };

  // |pref_filename| is the path to the prefs file we will try to load or save
  // to. Saves will be executed on |backend_thread|. It should be the file
  // thread in Chrome. You can pass NULL for unit tests, and then no separate
  // thread will be used.
  PrefService(const FilePath& pref_filename,
              const base::Thread* backend_thread);
  ~PrefService();

  // Reloads the data from file. This should only be called when the importer
  // is running during first run, and the main process may not change pref
  // values while the importer process is running. Returns true on success.
  bool ReloadPersistentPrefs();

  // Writes the data to disk. The return value only reflects whether
  // serialization was successful; we don't know whether the data actually made
  // it on disk (since it's on a different thread).  This should only be used if
  // we need to save immediately (basically, during shutdown).  Otherwise, you
  // should use ScheduleSavePersistentPrefs.
  bool SavePersistentPrefs();

  // Serializes the data and schedules save using ImportantFileWriter.
  bool ScheduleSavePersistentPrefs();

  DictionaryValue* transient() { return transient_.get(); }

  // Make the PrefService aware of a pref.
  void RegisterBooleanPref(const wchar_t* path,
                           bool default_value);
  void RegisterIntegerPref(const wchar_t* path,
                           int default_value);
  void RegisterRealPref(const wchar_t* path,
                        double default_value);
  void RegisterStringPref(const wchar_t* path,
                          const std::wstring& default_value);
  void RegisterFilePathPref(const wchar_t* path,
                            const FilePath& default_value);
  void RegisterListPref(const wchar_t* path);
  void RegisterDictionaryPref(const wchar_t* path);

  // These varients use a default value from the locale dll instead.
  void RegisterLocalizedBooleanPref(const wchar_t* path,
                                    int locale_default_message_id);
  void RegisterLocalizedIntegerPref(const wchar_t* path,
                                    int locale_default_message_id);
  void RegisterLocalizedRealPref(const wchar_t* path,
                                 int locale_default_message_id);
  void RegisterLocalizedStringPref(const wchar_t* path,
                                   int locale_default_message_id);

  // Returns whether the specified pref has been registered.
  bool IsPrefRegistered(const wchar_t* path);

  // If the path is valid and the value at the end of the path matches the type
  // specified, it will return the specified value.  Otherwise, the default
  // value (set when the pref was registered) will be returned.
  bool GetBoolean(const wchar_t* path) const;
  int GetInteger(const wchar_t* path) const;
  double GetReal(const wchar_t* path) const;
  std::wstring GetString(const wchar_t* path) const;
  FilePath GetFilePath(const wchar_t* path) const;

  // Returns the branch if it exists.  If it's not a branch or the branch does
  // not exist, returns NULL.  This does
  const DictionaryValue* GetDictionary(const wchar_t* path) const;
  const ListValue* GetList(const wchar_t* path) const;

  // If the pref at the given path changes, we call the observer's Observe
  // method with NOTIFY_PREF_CHANGED.
  void AddPrefObserver(const wchar_t* path, NotificationObserver* obs);
  void RemovePrefObserver(const wchar_t* path, NotificationObserver* obs);

  // Removes a user pref and restores the pref to its default value.
  void ClearPref(const wchar_t* path);

  // If the path is valid (i.e., registered), update the pref value.
  void SetBoolean(const wchar_t* path, bool value);
  void SetInteger(const wchar_t* path, int value);
  void SetReal(const wchar_t* path, double value);
  void SetString(const wchar_t* path, const std::wstring& value);
  void SetFilePath(const wchar_t* path, const FilePath& value);

  // Int64 helper methods that actually store the given value as a string.
  // Note that if obtaining the named value via GetDictionary or GetList, the
  // Value type will be TYPE_STRING.
  void SetInt64(const wchar_t* path, int64_t value);
  int64_t GetInt64(const wchar_t* path) const;
  void RegisterInt64Pref(const wchar_t* path, int64_t default_value);

  // Used to set the value of dictionary or list values in the pref tree.  This
  // will create a dictionary or list if one does not exist in the pref tree.
  // This method returns NULL only if you're requesting an unregistered pref or
  // a non-dict/non-list pref.
  // WARNING: Changes to the dictionary or list will not automatically notify
  // pref observers. TODO(tc): come up with a way to still fire observers.
  DictionaryValue* GetMutableDictionary(const wchar_t* path);
  ListValue* GetMutableList(const wchar_t* path);

  // Returns true if a value has been set for the specified path.
  // NOTE: this is NOT the same as IsPrefRegistered. In particular
  // IsPrefRegistered returns whether RegisterXXX has been invoked, where as
  // this checks if a value exists for the path.
  bool HasPrefPath(const wchar_t* path) const;

  class PreferencePathComparator {
   public:
    bool operator() (Preference* lhs, Preference* rhs) const {
      return lhs->name() < rhs->name();
    }
  };
  typedef std::set<Preference*, PreferencePathComparator> PreferenceSet;
  const PreferenceSet& preference_set() const { return prefs_; }

  // A helper method to quickly look up a preference.  Returns NULL if the
  // preference is not registered.
  const Preference* FindPreference(const wchar_t* pref_name) const;

 private:
  // Add a preference to the PreferenceMap.  If the pref already exists, return
  // false.  This method takes ownership of |pref|.
  void RegisterPreference(Preference* pref);

  // Returns a copy of the current pref value.  The caller is responsible for
  // deleting the returned object.
  Value* GetPrefCopy(const wchar_t* pref_name);

  // For the given pref_name, fire any observer of the pref.
  void FireObservers(const wchar_t* pref_name);

  // For the given pref_name, fire any observer of the pref only if |old_value|
  // is different from the current value.
  void FireObserversIfChanged(const wchar_t* pref_name,
                              const Value* old_value);

  // Serializes stored data to string. |output| is modified only
  // if serialization was successful. Returns true on success.
  bool SerializePrefData(std::string* output) const;

  scoped_ptr<DictionaryValue> persistent_;
  scoped_ptr<DictionaryValue> transient_;

  // Helper for safe writing pref data.
  ImportantFileWriter writer_;

  // A set of all the registered Preference objects.
  PreferenceSet prefs_;

  // A map from pref names to a list of observers.  Observers get fired in the
  // order they are added.
  typedef base::ObserverList<NotificationObserver> NotificationObserverList;
  typedef base::hash_map<std::wstring, NotificationObserverList*>
      PrefObserverMap;
  PrefObserverMap pref_observers_;

  DISALLOW_COPY_AND_ASSIGN(PrefService);
};

#endif  // CHROME_COMMON_PREF_SERVICE_H_
