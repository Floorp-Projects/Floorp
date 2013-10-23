/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "MiniShmParent.h"

#include "base/scoped_handle.h"

#include <sstream>

namespace mozilla {
namespace plugins {

// static
const unsigned int MiniShmParent::kDefaultMiniShmSectionSize = 0x1000;

MiniShmParent::MiniShmParent()
  : mSectionSize(0),
    mParentEvent(nullptr),
    mParentGuard(nullptr),
    mChildEvent(nullptr),
    mChildGuard(nullptr),
    mRegWait(nullptr),
    mFileMapping(nullptr),
    mView(nullptr),
    mIsConnected(false),
    mTimeout(INFINITE)
{
}

MiniShmParent::~MiniShmParent()
{
  CleanUp();
}

void
MiniShmParent::CleanUp()
{
  if (mRegWait) {
    ::UnregisterWaitEx(mRegWait, INVALID_HANDLE_VALUE);
    mRegWait = nullptr;
  }
  if (mParentEvent) {
    ::CloseHandle(mParentEvent);
    mParentEvent = nullptr;
  }
  if (mParentGuard) {
    ::CloseHandle(mParentGuard);
    mParentGuard = nullptr;
  }
  if (mChildEvent) {
    ::CloseHandle(mChildEvent);
    mChildEvent = nullptr;
  }
  if (mChildGuard) {
    ::CloseHandle(mChildGuard);
    mChildGuard = nullptr;
  }
  if (mView) {
    ::UnmapViewOfFile(mView);
    mView = nullptr;
  }
  if (mFileMapping) {
    ::CloseHandle(mFileMapping);
    mFileMapping = nullptr;
  }
}

nsresult
MiniShmParent::Init(MiniShmObserver* aObserver, const DWORD aTimeout,
                    const unsigned int aSectionSize)
{
  if (!aObserver || !aSectionSize || (aSectionSize % 0x1000) || !aTimeout) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (mFileMapping) {
    return NS_ERROR_ALREADY_INITIALIZED;
  }
  SECURITY_ATTRIBUTES securityAttributes = {sizeof(securityAttributes),
                                            nullptr,
                                            TRUE};
  ScopedHandle parentEvent(::CreateEvent(&securityAttributes,
                                         FALSE,
                                         FALSE,
                                         nullptr));
  if (!parentEvent.IsValid()) {
    return NS_ERROR_FAILURE;
  }
  ScopedHandle parentGuard(::CreateEvent(&securityAttributes,
                                         FALSE,
                                         TRUE,
                                         nullptr));
  if (!parentGuard.IsValid()) {
    return NS_ERROR_FAILURE;
  }
  ScopedHandle childEvent(::CreateEvent(&securityAttributes,
                                        FALSE,
                                        FALSE,
                                        nullptr));
  if (!childEvent.IsValid()) {
    return NS_ERROR_FAILURE;
  }
  ScopedHandle childGuard(::CreateEvent(&securityAttributes,
                                        FALSE,
                                        TRUE,
                                        nullptr));
  if (!childGuard.IsValid()) {
    return NS_ERROR_FAILURE;
  }
  ScopedHandle mapping(::CreateFileMapping(INVALID_HANDLE_VALUE,
                                           &securityAttributes,
                                           PAGE_READWRITE,
                                           0,
                                           aSectionSize,
                                           nullptr));
  if (!mapping.IsValid()) {
    return NS_ERROR_FAILURE;
  }
  ScopedMappedFileView view(::MapViewOfFile(mapping,
                                            FILE_MAP_WRITE,
                                            0, 0, 0));
  if (!view.IsValid()) {
    return NS_ERROR_FAILURE;
  }
  nsresult rv = SetView(view, aSectionSize, false);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = SetGuard(childGuard, aTimeout);
  NS_ENSURE_SUCCESS(rv, rv);

  MiniShmInit* initStruct = nullptr;
  rv = GetWritePtrInternal(initStruct);
  NS_ENSURE_SUCCESS(rv, rv);
  initStruct->mParentEvent = parentEvent;
  initStruct->mParentGuard = parentGuard;
  initStruct->mChildEvent = childEvent;
  initStruct->mChildGuard = childGuard;

  if (!::RegisterWaitForSingleObject(&mRegWait,
                                     parentEvent,
                                     &SOnEvent,
                                     this,
                                     INFINITE,
                                     WT_EXECUTEDEFAULT)) {
    return NS_ERROR_FAILURE;
  }

  mParentEvent = parentEvent.Take();
  mParentGuard = parentGuard.Take();
  mChildEvent = childEvent.Take();
  mChildGuard = childGuard.Take();
  mFileMapping = mapping.Take();
  mView = view.Take();
  mSectionSize = aSectionSize;
  SetObserver(aObserver);
  mTimeout = aTimeout;
  return NS_OK;
}

nsresult
MiniShmParent::GetCookie(std::wstring& cookie)
{
  if (!mFileMapping) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  std::wostringstream oss;
  oss << mFileMapping;
  if (!oss) {
    return NS_ERROR_FAILURE;
  }
  cookie = oss.str();
  return NS_OK;
}

nsresult
MiniShmParent::Send()
{
  if (!mChildEvent) {
    return NS_ERROR_NOT_INITIALIZED;
  }
  if (!::SetEvent(mChildEvent)) {
    return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

bool
MiniShmParent::IsConnected() const
{
  return mIsConnected;
}

void
MiniShmParent::OnEvent()
{
  if (mIsConnected) {
    MiniShmBase::OnEvent();
  } else {
    FinalizeConnection();
  }
  ::SetEvent(mParentGuard);
}

void
MiniShmParent::FinalizeConnection()
{
  const MiniShmInitComplete* initCompleteStruct = nullptr;
  nsresult rv = GetReadPtr(initCompleteStruct);
  mIsConnected = NS_SUCCEEDED(rv) && initCompleteStruct->mSucceeded;
  if (mIsConnected) {
    OnConnect();
  }
}

} // namespace plugins
} // namespace mozilla

