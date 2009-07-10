/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: sw=4 ts=4 et : */

#include "TabProcessParent.h"

using mozilla::ipc::GeckoChildProcessHost;

namespace mozilla {
namespace tabs {


TabProcessParent::TabProcessParent() :
    GeckoChildProcessHost(GeckoChildProcess_Tab)
{
}

TabProcessParent::~TabProcessParent()
{
}


} // namespace tabs
} // namespace mozilla
