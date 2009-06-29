// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// FieldTrial is a class for handling details of statistical experiments
// performed by actual users in the field (i.e., in a shipped or beta product).
// All code is called exclusively on the UI thread currently.
//
// The simplest example is an experiment to see whether one of two options
// produces "better" results across our user population.  In that scenario, UMA
// data is uploaded to aggregate the test results, and this FieldTrial class
// manages the state of each such experiment (state == which option was
// pseudo-randomly selected).
//
// States are typically generated randomly, either based on a one time
// randomization (generated randomly once, and then persistently reused in the
// client during each future run of the program), or by a startup randomization
// (generated each time the application starts up, but held constant during the
// duration of the process), or by continuous randomization across a run (where
// the state can be recalculated again and again, many times during a process).
// Only startup randomization is implemented thus far.

//------------------------------------------------------------------------------
// Example:  Suppose we have an experiment involving memory, such as determining
// the impact of memory model command line flags actual memory use.
// We assume that we already have a histogram of memory usage, such as:

//   HISTOGRAM_COUNTS("Memory.RendererTotal", count);

// Somewhere in main thread initialization code, we'd probably define an
// instance of a FieldTrial, with code such as:

// // Note, FieldTrials are reference counted, and persist automagically until
// // process teardown, courtesy of their automatic registration in
// // FieldTrialList.
// scoped_refptr<FieldTrial> trial = new FieldTrial("MemoryExperiment", 1000);
// int group1 = trial->AppendGroup("_high_mem", 20);  // 2% in _high_mem group.
// int group2 = trial->AppendGroup("_low_mem", 20);   // 2% in _low_mem group.
// // Take action depending of which group we randomly land in.
// if (trial->group() == group1)
//   SetMemoryModel(HIGH);  // Sample setting of browser state.
// else if (trial->group() == group2)
//   SetMemoryModel(LOW);  // Sample alternate setting.

// We then modify any histograms we wish to correlate with our experiment to
// have slighly different names, depending on what group the trial instance
// happened (randomly) to be assigned to:

// HISTOGRAM_COUNTS(FieldTrial::MakeName("Memory.RendererTotal",
//                                       "MemoryExperiment").data(), count);

// The above code will create 3 distinct histograms, with each run of the
// application being assigned to of of teh three groups, and for each group, the
// correspondingly named histogram will be populated:

// Memory.RendererTotal            // 96% of users still fill this histogram.
// Memory.RendererTotal_high_mem   // 2% of users will fill this histogram.
// Memory.RendererTotal_low_mem    // 2% of users will fill this histogram.

//------------------------------------------------------------------------------

#ifndef BASE_FIELD_TRIAL_H_
#define BASE_FIELD_TRIAL_H_

#include <map>
#include <string>

#include "base/lock.h"
#include "base/non_thread_safe.h"
#include "base/ref_counted.h"
#include "base/time.h"


class FieldTrial : public base::RefCounted<FieldTrial> {
 public:
  static const int kNotParticipating;

  typedef int Probability;  // Use scaled up probability.

  // The name is used to register the instance with the FieldTrialList class,
  // and can be used to find the trial (only one trial can be present for each
  // name).
  // Group probabilities that are later supplied must sum to less than or equal
  // to the total_probability.
  FieldTrial(const std::string& name, Probability total_probability);

  // Establish the name and probability of the next group in this trial.
  // Sometimes, based on construction randomization, this call may causes the
  // provided group to be *THE* group selected for use in this instance.
  int AppendGroup(const std::string& name, Probability group_probability);

  // Return the name of the FieldTrial (excluding the group name).
  std::string name() const { return name_; }

  // Return the randomly selected group number that was assigned.
  // Return kNotParticipating if the instance is not participating in the
  // experiment.
  int group() const { return group_; }

  // If the field trial is not in an experiment, this returns the empty string.
  // if the group's name is empty, a name of "_" concatenated with the group
  // number is used as the group name.
  std::string group_name() const { return group_name_; }

  // Helper function for the most common use: as an argument to specifiy the
  // name of a HISTOGRAM.  Use the original histogram name as the name_prefix.
  static std::string MakeName(const std::string& name_prefix,
                              const std::string& trial_name);

 private:
  // The name of the field trial, as can be found via the FieldTrialList.
  // This is empty of the trial is not in the experiment.
  const std::string name_;

  // The maximu sum of all probabilities supplied, which corresponds to 100%.
  // This is the scaling factor used to adjust supplied probabilities.
  Probability divisor_;

  // The randomly selected probability that is used to select a group (or have
  // the instance not participate).  It is the product of divisor_ and a random
  // number between [0, 1).
  Probability random_;

  // Sum of the probabilities of all appended groups.
  Probability accumulated_group_probability_;

  int next_group_number_;

  // The pseudo-randomly assigned group number.
  // This is kNotParticipating if no group has been assigned.
  int group_;

  // A textual name for the randomly selected group, including the Trial name.
  // If this Trial is not a member of an group, this string is empty.
  std::string group_name_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrial);
};

//------------------------------------------------------------------------------
// Class with a list of all active field trials.  A trial is active if it has
// been registered, which includes evaluating its state based on its probaility.
// Only one instance of this class exists.
class FieldTrialList {
 public:
  // This singleton holds the global list of registered FieldTrials.
  FieldTrialList();
  // Destructor Release()'s references to all registered FieldTrial instances.
  ~FieldTrialList();

  // Register() stores a pointer to the given trial in a global map.
  // This method also AddRef's the indicated trial.
  static void Register(FieldTrial* trial);

  // The Find() method can be used to test to see if a named Trial was already
  // registered, or to retrieve a pointer to it from the global map.
  static FieldTrial* Find(const std::string& name);

  static int FindValue(const std::string& name);

  static std::string FindFullName(const std::string& name);

  // The time of construction of the global map is recorded in a static variable
  // and is commonly used by experiments to identify the time since the start
  // of the application.  In some experiments it may be useful to discount
  // data that is gathered before the application has reached sufficient
  // stability (example: most DLL have loaded, etc.)
  static base::Time application_start_time() {
    if (global_)
      return global_->application_start_time_;
    // For testing purposes only, or when we don't yet have a start time.
    return base::Time::Now();
  }

 private:
  // Helper function should be called only while holding lock_.
  FieldTrial* PreLockedFind(const std::string& name);

  typedef std::map<std::string, FieldTrial*> RegistrationList;

  static FieldTrialList* global_;  // The singleton of this class.

  base::Time application_start_time_;

  // Lock for access to registered_.
  Lock lock_;
  RegistrationList registered_;

  DISALLOW_COPY_AND_ASSIGN(FieldTrialList);
};

#endif  // BASE_FIELD_TRIAL_H_
