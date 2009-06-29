// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/property_bag.h"

PropertyBag::PropertyBag() {
}

PropertyBag::PropertyBag(const PropertyBag& other) {
  operator=(other);
}

PropertyBag::~PropertyBag() {
}

PropertyBag& PropertyBag::operator=(const PropertyBag& other) {
  props_.clear();

  // We need to make copies of each property using the virtual copy() method.
  for (PropertyMap::const_iterator i = other.props_.begin();
       i != other.props_.end(); ++i)
    props_[i->first] = linked_ptr<Prop>(i->second->copy());
  return *this;
}

void PropertyBag::SetProperty(PropID id, Prop* prop) {
  props_[id] = linked_ptr<Prop>(prop);
}

PropertyBag::Prop* PropertyBag::GetProperty(PropID id) {
  PropertyMap::const_iterator found = props_.find(id);
  if (found == props_.end())
    return NULL;
  return found->second.get();
}

const PropertyBag::Prop* PropertyBag::GetProperty(PropID id) const {
  PropertyMap::const_iterator found = props_.find(id);
  if (found == props_.end())
    return NULL;
  return found->second.get();
}

void PropertyBag::DeleteProperty(PropID id) {
  PropertyMap::iterator found = props_.find(id);
  if (found == props_.end())
    return;  // Not found, nothing to do.
  props_.erase(found);
}

PropertyAccessorBase::PropertyAccessorBase() {
  static PropertyBag::PropID next_id = 1;
  prop_id_ = next_id++;
}
