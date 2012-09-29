/* -*- Mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; tab-width: 40 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_dom_bluetooth_bluetootheventservice_h__
#define mozilla_dom_bluetooth_bluetootheventservice_h__

#include "BluetoothCommon.h"
#include "nsAutoPtr.h"
#include "nsClassHashtable.h"
#include "nsIObserver.h"
#include "nsIThread.h"
#include "nsTObserverArray.h"

namespace mozilla {
namespace ipc {
class UnixSocketConsumer;
}
}

BEGIN_BLUETOOTH_NAMESPACE

class BluetoothManager;
class BluetoothNamedValue;
class BluetoothReplyRunnable;
class BluetoothSignal;

typedef mozilla::ObserverList<BluetoothSignal> BluetoothSignalObserverList;

class BluetoothService : public nsIObserver
                       , public BluetoothSignalObserver
{
  class ToggleBtAck;
  friend class ToggleBtAck;

  class ToggleBtTask;
  friend class ToggleBtTask;

  class StartupTask;
  friend class StartupTask;

public:
  NS_DECL_ISUPPORTS
  NS_DECL_NSIOBSERVER

  /** 
   * Add a message handler object from message distribution observer.
   * Must be called from the main thread.
   *
   * @param aNodeName Node name of the object
   * @param aMsgHandler Weak pointer to the object
   */
  virtual void
  RegisterBluetoothSignalHandler(const nsAString& aNodeName,
                                 BluetoothSignalObserver* aMsgHandler);

  /** 
   * Remove a message handler object from message distribution observer.
   * Must be called from the main thread.
   *
   * @param aNodeName Node name of the object
   * @param aMsgHandler Weak pointer to the object
   */
  virtual void
  UnregisterBluetoothSignalHandler(const nsAString& aNodeName,
                                   BluetoothSignalObserver* aMsgHandler);

  /** 
   * Remove a message handlers for the given observer.
   * Must be called from the main thread.
   *
   * @param aMsgHandler Weak pointer to the object
   */
  void
  UnregisterAllSignalHandlers(BluetoothSignalObserver* aMsgHandler);

  /** 
   * Distribute a signal to the observer list
   *
   * @param aSignal Signal object to distribute
   *
   * @return NS_OK if signal distributed, NS_ERROR_FAILURE on error
   */
  void
  DistributeSignal(const BluetoothSignal& aEvent);

  /**
   * Called when a BluetoothManager is created.
   */
  void
  RegisterManager(BluetoothManager* aManager);

  /**
   * Called when a BluetoothManager is destroyed.
   */
  void
  UnregisterManager(BluetoothManager* aManager);

  /**
   * Called when get a Bluetooth Signal from BluetoothDBusService
   *
   */
  void
  Notify(const BluetoothSignal& aParam);

  /**
   * Returns the BluetoothService singleton. Only to be called from main thread.
   *
   * @param aService Pointer to return singleton into.
   *
   * @return NS_OK on proper assignment, NS_ERROR_FAILURE otherwise (if service
   * has not yet been started, for instance)
   */
  static BluetoothService*
  Get();

  static already_AddRefed<BluetoothService>
  FactoryCreate()
  {
    nsRefPtr<BluetoothService> service = Get();
    return service.forget();
  }

  /**
   * Returns the path of the default adapter, implemented via a platform
   * specific method.
   *
   * @return Default adapter path/name on success, NULL otherwise
   */
  virtual nsresult
  GetDefaultAdapterPathInternal(BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Returns the properties of paired devices, implemented via a platform
   * specific method.
   *
   * @return NS_OK on success, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  GetPairedDevicePropertiesInternal(const nsTArray<nsString>& aDeviceAddresses,
                                    BluetoothReplyRunnable* aRunnable) = 0;

  /** 
   * Stop device discovery (platform specific implementation)
   *
   * @param aAdapterPath Adapter to stop discovery on
   *
   * @return NS_OK if discovery stopped correctly, false otherwise
   */
  virtual nsresult
  StopDiscoveryInternal(const nsAString& aAdapterPath,
                        BluetoothReplyRunnable* aRunnable) = 0;

  /** 
   * Start device discovery (platform specific implementation)
   *
   * @param aAdapterPath Adapter to start discovery on
   *
   * @return NS_OK if discovery stopped correctly, false otherwise
   */
  virtual nsresult
  StartDiscoveryInternal(const nsAString& aAdapterPath,
                         BluetoothReplyRunnable* aRunnable) = 0;

  /**
   * Fetches the propertes for the specified object
   *
   * @param aType Type of the object (see BluetoothObjectType in BluetoothCommon.h)
   * @param aPath Path of the object
   * @param aRunnable Runnable to return to after receiving callback
   *
   * @return NS_OK on function run, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  GetProperties(BluetoothObjectType aType,
                const nsAString& aPath,
                BluetoothReplyRunnable* aRunnable) = 0;

  /** 
   * Fetches the propertes for the specified device
   *
   * @param aSignal BluetoothSignal to be distrubuted after retrieving device properties
   *
   * @return NS_OK on function run, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  GetDevicePropertiesInternal(const BluetoothSignal& aSignal) = 0;

  /**
   * Set a property for the specified object
   *
   * @param aPath Path to the object
   * @param aPropName Name of the property
   * @param aValue Boolean value
   * @param aRunnable Runnable to run on async reply
   *
   * @return NS_OK if property is set correctly, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  SetProperty(BluetoothObjectType aType,
              const nsAString& aPath,
              const BluetoothNamedValue& aValue,
              BluetoothReplyRunnable* aRunnable) = 0;

  /** 
   * Get the path of a device
   *
   * @param aAdapterPath Path to the Adapter that's communicating with the device
   * @param aDeviceAddress Device address (XX:XX:XX:XX:XX:XX format)
   * @param aDevicePath Return value of path
   *
   * @return True if path set correctly, false otherwise
   */
  virtual bool
  GetDevicePath(const nsAString& aAdapterPath,
                const nsAString& aDeviceAddress,
                nsAString& aDevicePath) = 0;

  virtual nsresult
  CreatePairedDeviceInternal(const nsAString& aAdapterPath,
                             const nsAString& aAddress,
                             int aTimeout,
                             BluetoothReplyRunnable* aRunnable) = 0;

  virtual nsresult
  RemoveDeviceInternal(const nsAString& aAdapterPath,
                       const nsAString& aObjectPath,
                       BluetoothReplyRunnable* aRunnable) = 0;

  virtual nsresult
  GetScoSocket(const nsAString& aObjectPath,
               bool aAuth,
               bool aEncrypt,
               mozilla::ipc::UnixSocketConsumer* aConsumer) = 0;

  virtual nsresult
  GetSocketViaService(const nsAString& aObjectPath,
                      const nsAString& aService,
                      BluetoothSocketType aType,
                      bool aAuth,
                      bool aEncrypt,
                      mozilla::ipc::UnixSocketConsumer* aSocketConsumer,
                      BluetoothReplyRunnable* aRunnable) = 0;

  virtual bool
  SetPinCodeInternal(const nsAString& aDeviceAddress, const nsAString& aPinCode,
                     BluetoothReplyRunnable* aRunnable) = 0;

  virtual bool
  SetPasskeyInternal(const nsAString& aDeviceAddress, uint32_t aPasskey,
                     BluetoothReplyRunnable* aRunnable) = 0;

  virtual bool
  SetPairingConfirmationInternal(const nsAString& aDeviceAddress, bool aConfirm,
                                 BluetoothReplyRunnable* aRunnable) = 0;

  virtual bool
  SetAuthorizationInternal(const nsAString& aDeviceAddress, bool aAllow,
                           BluetoothReplyRunnable* aRunnable) = 0;

  virtual nsresult
  PrepareAdapterInternal(const nsAString& aPath) = 0;

  virtual bool
  ConnectHeadset(const nsAString& aDeviceAddress,
                 const nsAString& aAdapterPath,
                 BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  DisconnectHeadset(BluetoothReplyRunnable* aRunnable) = 0;

  virtual bool
  ConnectObjectPush(const nsAString& aDeviceAddress,
                    const nsAString& aAdapterPath,
                    BluetoothReplyRunnable* aRunnable) = 0;

  virtual void
  DisconnectObjectPush(BluetoothReplyRunnable* aRunnable) = 0;

  bool
  IsEnabled() const
  {
    return mEnabled;
  }

protected:
  BluetoothService()
  : mEnabled(false), mSettingsCheckInProgress(false),
    mRegisteredForLocalAgent(false)
#ifdef DEBUG
    , mLastRequestedEnable(false)
#endif
  {
    mBluetoothSignalObserverTable.Init();
  }

  virtual ~BluetoothService();

  bool
  Init();

  void
  Cleanup();

  nsresult
  StartStopBluetooth(bool aStart);

  /** 
   * Platform specific startup functions go here. Usually deals with member
   * variables, so not static. Guaranteed to be called outside of main thread.
   *
   * @return NS_OK on correct startup, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  StartInternal() = 0;

  /** 
   * Platform specific startup functions go here. Usually deals with member
   * variables, so not static. Guaranteed to be called outside of main thread.
   *
   * @return NS_OK on correct startup, NS_ERROR_FAILURE otherwise
   */
  virtual nsresult
  StopInternal() = 0;

  /**
   * Called when XPCOM first creates this service.
   */
  virtual nsresult
  HandleStartup();

  /**
   * Called when the startup settings check has completed.
   */
  nsresult
  HandleStartupSettingsCheck(bool aEnable);

  /**
   * Called when "mozsettings-changed" observer topic fires.
   */
  nsresult
  HandleSettingsChanged(const nsAString& aData);

  /**
   * Called when XPCOM is shutting down.
   */
  virtual nsresult
  HandleShutdown();

  // Called by ToggleBtAck.
  void
  SetEnabled(bool aEnabled);

  // Called by Get().
  static BluetoothService*
  Create();

  /**
   * Due to the fact that some operations require multiple calls, a
   * CommandThread is created that can run blocking, platform-specific calls
   * where either no asynchronous equivilent exists, or else where multiple
   * asynchronous calls would require excessive runnable bouncing between main
   * thread and IO thread.
   *
   * For instance, when we retrieve an Adapter object, we would like it to come
   * with all of its properties filled in and registered as an agent, which
   * requires a minimum of 3 calls to platform specific code on some platforms.
   *
   */
  nsCOMPtr<nsIThread> mBluetoothCommandThread;

  typedef nsClassHashtable<nsStringHashKey, BluetoothSignalObserverList >
  BluetoothSignalObserverTable;

  BluetoothSignalObserverTable mBluetoothSignalObserverTable;

  typedef nsTObserverArray<BluetoothManager*> BluetoothManagerList;
  BluetoothManagerList mLiveManagers;

  bool mEnabled;
  bool mSettingsCheckInProgress;
  bool mRegisteredForLocalAgent;

#ifdef DEBUG
  bool mLastRequestedEnable;
#endif
};

END_BLUETOOTH_NAMESPACE

#endif
