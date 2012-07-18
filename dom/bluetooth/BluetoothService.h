/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=40: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetootheventservice_h__
#define mozilla_dom_bluetooth_bluetootheventservice_h__

#include "nsThreadUtils.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"
#include "BluetoothCommon.h"

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothSignal;
class BluetoothReplyRunnable;
class BluetoothNamedValue;

class BluetoothService : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /** 
   * Add a message handler object from message distribution observer.
   * Must be called from the main thread.
   *
   * @param aNodeName Node name of the object
   * @param aMsgHandler Weak pointer to the object
   *
   * @return NS_OK on successful addition to observer, NS_ERROR_FAILED
   * otherwise
   */
  nsresult RegisterBluetoothSignalHandler(const nsAString& aNodeName,
                                          BluetoothSignalObserver* aMsgHandler);

  /** 
   * Remove a message handler object from message distribution observer.
   * Must be called from the main thread.
   *
   * @param aNodeName Node name of the object
   * @param aMsgHandler Weak pointer to the object
   *
   * @return NS_OK on successful removal from observer service,
   * NS_ERROR_FAILED otherwise
   */
  nsresult UnregisterBluetoothSignalHandler(const nsAString& aNodeName,
                                            BluetoothSignalObserver* aMsgHandler);

  /** 
   * Distribute a signal to the observer list
   *
   * @param aSignal Signal object to distribute
   *
   * @return NS_OK if signal distributed, NS_ERROR_FAILURE on error
   */
  nsresult DistributeSignal(const BluetoothSignal& aEvent);

  /** 
   * Start bluetooth services. Starts up any threads and connections that
   * bluetooth needs to operate on the current platform. Assumed to be run on
   * the main thread with delayed return for blocking startup functions via
   * runnable.
   *
   * @param aResultRunnable Runnable object to execute after bluetooth has
   * either come up or failed. Runnable should check existence of
   * BluetoothService via Get() function to see if service started correctly.
   *
   * @return NS_OK on initialization starting correctly, NS_ERROR_FAILURE
   * otherwise
   */
  nsresult Start(BluetoothReplyRunnable* aResultRunnable);

  /** 
   * Stop bluetooth services. Starts up any threads and connections that
   * bluetooth needs to operate on the current platform. Assumed to be run on
   * the main thread with delayed return for blocking startup functions via
   * runnable.
   *
   * @param aResultRunnable Runnable object to execute after bluetooth has
   * either shut down or failed. Runnable should check lack of existence of
   * BluetoothService via Get() function to see if service stopped correctly.
   *
   * @return NS_OK on initialization starting correctly, NS_ERROR_FAILURE
   * otherwise
   */
  nsresult Stop(BluetoothReplyRunnable* aResultRunnable);

  /** 
   * Returns the BluetoothService singleton. Only to be called from main thread.
   *
   * @param aService Pointer to return singleton into. 
   *
   * @return NS_OK on proper assignment, NS_ERROR_FAILURE otherwise (if service
   * has not yet been started, for instance)
   */
  static BluetoothService* Get();
  
  /**
   * Returns the path of the default adapter, implemented via a platform
   * specific method.
   *
   * @return Default adapter path/name on success, NULL otherwise
   */
  virtual nsresult GetDefaultAdapterPathInternal(BluetoothReplyRunnable* aRunnable) = 0;

  /** 
   * Stop device discovery (platform specific implementation)
   *
   * @param aAdapterPath Adapter to stop discovery on
   *
   * @return NS_OK if discovery stopped correctly, false otherwise
   */
  virtual nsresult StopDiscoveryInternal(const nsAString& aAdapterPath,
                                         BluetoothReplyRunnable* aRunnable) = 0;

  /** 
   * Start device discovery (platform specific implementation)
   *
   * @param aAdapterPath Adapter to start discovery on
   *
   * @return NS_OK if discovery stopped correctly, false otherwise
   */
  virtual nsresult StartDiscoveryInternal(const nsAString& aAdapterPath,
                                          BluetoothReplyRunnable* aRunnable) = 0;

  /** 
   * Platform specific startup functions go here. Usually deals with member
   * variables, so not static. Guaranteed to be called outside of main thread.
   *
   * @return NS_OK on correct startup, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult StartInternal() = 0;

  /** 
   * Platform specific startup functions go here. Usually deals with member
   * variables, so not static. Guaranteed to be called outside of main thread.
   *
   * @return NS_OK on correct startup, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult StopInternal() = 0;

protected:
  BluetoothService()
  {
    mBluetoothSignalObserverTable.Init();
  }

  virtual ~BluetoothService()
  {
  }

  nsresult StartStopBluetooth(BluetoothReplyRunnable* aResultRunnable,
                              bool aStart);
  // This function is implemented in platform-specific BluetoothServiceFactory
  // files
  static BluetoothService* Create();

  typedef mozilla::ObserverList<BluetoothSignal> BluetoothSignalObserverList;
  typedef nsClassHashtable<nsStringHashKey, BluetoothSignalObserverList >
  BluetoothSignalObserverTable;

  BluetoothSignalObserverTable mBluetoothSignalObserverTable;
};

END_BLUETOOTH_NAMESPACE

#endif
