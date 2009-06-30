/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: sw=4 ts=4 et : */

#include "TabProcessParent.h"

#include "chrome/common/chrome_switches.h"

namespace mozilla {
namespace tabs {

char const *const TabProcessParent::kTabProcessName = "gecko-iframe" BIN_SUFFIX;

TabProcessParent::TabProcessParent()
{
}

TabProcessParent::~TabProcessParent()
{
}

bool TabProcessParent::Launch()
{
    if (!CreateChannel())
        return false;

    FilePath exePath =
        FilePath::FromWStringHack(CommandLine::ForCurrentProcess()->program());
    exePath = exePath.DirName();
    exePath = exePath.AppendASCII(kTabProcessName);

#if defined(OS_POSIX)
    int srcChannelFd, dstChannelFd;
    channel().GetClientFileDescriptorMapping(&srcChannelFd, &dstChannelFd);
    mFileMap.push_back(std::pair<int,int>(srcChannelFd, dstChannelFd));
#endif

    CommandLine cmdLine(exePath.ToWStringHack());
    cmdLine.AppendSwitchWithValue(switches::kProcessChannelID, channel_id());

    base::ProcessHandle process;
#if defined(OS_POSIX)
    base::LaunchApp(cmdLine.argv(), mFileMap, false, &process);
#else
#error Loser
#endif

    if (!process)
        return false;

    SetHandle(process);
    return true;
}

} // namespace tabs
} // namespace mozilla
