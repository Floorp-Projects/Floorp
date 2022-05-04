# GeckoView Thread Topography
Unlike conventional Gecko, GeckoView utilizes two main threads in the parent process. A thread called the "UI" thread that runs the native Java event loop, and a "Gecko" thread that spawns an instance of Gecko. Note, this "Gecko" thread is considered the main thread in conventional Gecko and throughout the source is referred to as such. For example `NS_IsMainThread()` returns `true` when called on the Gecko thread.

Android's accessibility API's entry point is the UI thread. The most common methods for an accessibility service to query and interact with Gecko have a return value and thus must be performed synchronously. For example [performAction()](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeProvider#performAction(int,%20int,%20android.os.Bundle)) and [createAccessibilityNodeInfo()](https://developer.android.com/reference/android/view/accessibility/AccessibilityNodeProvider#createAccessibilityNodeInfo(int)).

## Thread Safety Concerns
As a rule, every data structure and method in Gecko should be considered unsafe to access from GeckoView's UI thread. For example the default reference counting implementation is not thread safe and should not be used outside the Gecko thread. Cross-process messaging must be done from the Gecko thread as well.

Since the accessibility consumer is in the UI thread we need to either call into the Gecko thread while blocking the UI thread, or make a limited subset of the accessibility API thread safe. We employ both methods for different types of content.

## In-Process Content
Content that is rendered in the top-level process is rare and mostly consists of "about" pages that give insight to browser internals, like `about:support`.

Since our Gecko accessibility API queries DOM and Layout, and includes many complex, thread-unsafe data structures, we call into the Gecko thread to retrieve what is needed and block the UI until we get a response from the Gecko thread. For example, to retrieve a "class name" enum that is calculated for a certain accessible the method will look something like this:
```cpp
int SessionAccessibility::GetNodeClassName(int32_t aID) {
  int32_t classNameEnum = java::SessionAccessibility::CLASSNAME_VIEW;
  RefPtr<SessionAccessibility> self(this);
  nsAppShell::SyncRunEvent([this, self, aID, &classNameEnum] {
    if (Accessible* acc = GetAccessibleByID(aID)) {
      classNameEnum = AccessibleWrap::AndroidClass(acc);
    }
  });

  return classNameEnum;
}
```

## Out-of-Process Content
Most web content will be rendered in a child process. Historically, we would cache the accessible tree hierarchy and object roles in the parent process and use synchronous IPC to query individual objects for more properties. We are transitioning to a fully cached approach where all fields are either stored or calculated in remote proxy objects in the parent process.

The "cache" in our definition is any data member associated with the remote `Accessible` or its subclasses, such as `mParent`, `mChildren` and `mCachedFields` in `RemoteAccessibleBase`, or `mAccessibles` in `DocAccessibleParent`. In addition to all those members the cache includes the accessible/id table in `SessionAccessibility`.

The cache is initialized and modified exclusively by messages from the child process to the parent process. Since the cache is limited to our module and is relatively well scoped, it is possible to synchronize access to it and allow it to be consumed by the UI thread.

### Global Accessibility Thread Monitor
We have global thread monitor that can be acquired by calling `nsAccessibilityService::GetAndroidMonitor()`. For example, if we were to retrieve the class name enum we would use an RAII wrapped monitor to exclusively access the cache:
```cpp
int SessionAccessibility::GetNodeClassName(int32_t aID) {
  MonitorAutoLock mal(nsAccessibilityService::GetAndroidMonitor());
  if (Accessible* acc = GetAccessibleByID(aID)) {
    return AccessibleWrap::AndroidClass(acc);
  }
}
```

#### Gecko Thread Access
As a rule, all incoming IPC messages (ie. all `DocAccessibleParent::Recv` methods) should hold the global monitor. This is because any `Recv` method may indirectly modify the cache, for example a certain event might invalidate or update a cached property. There are currently no deferred tasks that happen as a result of a `Recv` method, so holding the monitor at the start of these methods will assure that the cache is being read or written in a synchronized fashion.

#### UI Thread Access
All methods that are called in via JNI should be assumed are in the UI thread and should acquire the monitor, unless the are annotated with `@WrapForJNI(dispatchTo = "gecko")`. This assures us that they are synchronized with any potential modifications to the cache happening in the Gecko thread.
