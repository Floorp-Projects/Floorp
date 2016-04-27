#include "MediaCallbackID.h"

namespace mozilla {

char const* CallbackID::INVALID_TAG = "INVALID_TAG";
int32_t const CallbackID::INVALID_ID = -1;

CallbackID::CallbackID()
  : mTag(INVALID_TAG), mID(INVALID_ID)
{
}

CallbackID::CallbackID(char const* aTag, int32_t aID /* = 0*/)
  : mTag(aTag), mID(aID)
{
}

CallbackID&
CallbackID::operator++()
{
  ++mID;
  return *this;
}

CallbackID
CallbackID::operator++(int)
{
  CallbackID ret = *this;
  ++(*this); // call prefix++
  return ret;
}

bool
CallbackID::operator==(const CallbackID& rhs) const
{
  return (strcmp(mTag, rhs.mTag) == 0) && (mID == rhs.mID);
}

bool
CallbackID::operator!=(const CallbackID& rhs) const
{
  return !(*this == rhs);
}

CallbackID::operator int() const
{
  return mID;
}

} // namespace mozilla
