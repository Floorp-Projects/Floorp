/* -*- Mode: C; tab-width: 8 -*-
 * Copyright (C) 1998 Netscape Communications Corporation, All Rights Reserved.
 */

/*
	OSStatusException.h
	
	MacOS OSStatus exception.
	
	by Patrick C. Beard.
 */

#pragma once

#ifndef __TYPES__
#include <Types.h>
#endif

class OSStatusException {
public:
	OSStatusException(OSStatus status) : mStatus(status) {}
	OSStatus getStatus() { return mStatus; }

private:
	OSStatus mStatus;
};

inline void checkStatus(OSStatus status)
{
	if (status != noErr)
		throw OSStatusException(status);
}
