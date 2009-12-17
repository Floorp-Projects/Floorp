// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#include "base/field_trial.h"
#include "base/logging.h"
#include "base/rand_util.h"
#include "base/string_util.h"

using base::Time;

// static
const int FieldTrial::kNotParticipating = -1;

//------------------------------------------------------------------------------
// FieldTrial methods and members.

FieldTrial::FieldTrial(const std::string& name,
                       const Probability total_probability)
  : name_(name),
    divisor_(total_probability),
    random_(static_cast<Probability>(divisor_ * base::RandDouble())),
    accumulated_group_probability_(0),
    next_group_number_(0),
    group_(kNotParticipating) {
  FieldTrialList::Register(this);
}

int FieldTrial::AppendGroup(const std::string& name,
                            Probability group_probability) {
  DCHECK(group_probability <= divisor_);
  accumulated_group_probability_ += group_probability;
  DCHECK(accumulated_group_probability_ <= divisor_);
  if (group_ == kNotParticipating && accumulated_group_probability_ > random_) {
    // This is the group that crossed the random line, so we do teh assignment.
    group_ = next_group_number_;
    if (name.empty())
      StringAppendF(&group_name_, "_%d", group_);
    else
      group_name_ = name;
  }
  return next_group_number_++;
}

// static
std::string FieldTrial::MakeName(const std::string& name_prefix,
                                 const std::string& trial_name) {
  std::string big_string(name_prefix);
  return big_string.append(FieldTrialList::FindFullName(trial_name));
}

//------------------------------------------------------------------------------
// FieldTrialList methods and members.

// static
FieldTrialList* FieldTrialList::global_ = NULL;

FieldTrialList::FieldTrialList()
  : application_start_time_(Time::Now()) {
  DCHECK(!global_);
  global_ = this;
}

FieldTrialList::~FieldTrialList() {
  AutoLock auto_lock(lock_);
  while (!registered_.empty()) {
    RegistrationList::iterator it = registered_.begin();
    it->second->Release();
    registered_.erase(it->first);
  }
  DCHECK(this == global_);
  global_ = NULL;
}

// static
void FieldTrialList::Register(FieldTrial* trial) {
  AutoLock auto_lock(global_->lock_);
  DCHECK(!global_->PreLockedFind(trial->name()));
  trial->AddRef();
  global_->registered_[trial->name()] = trial;
}

// static
int FieldTrialList::FindValue(const std::string& name) {
  FieldTrial* field_trial = Find(name);
  if (field_trial)
    return field_trial->group();
  return FieldTrial::kNotParticipating;
}

// static
std::string FieldTrialList::FindFullName(const std::string& name) {
  FieldTrial* field_trial = Find(name);
  if (field_trial)
    return field_trial->group_name();
  return "";
}

// static
FieldTrial* FieldTrialList::Find(const std::string& name) {
  if (!global_)
    return NULL;
  AutoLock auto_lock(global_->lock_);
  return global_->PreLockedFind(name);
}

FieldTrial* FieldTrialList::PreLockedFind(const std::string& name) {
  RegistrationList::iterator it = registered_.find(name);
  if (registered_.end() == it)
    return NULL;
  return it->second;
}
