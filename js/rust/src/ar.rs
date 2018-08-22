use jsapi::root::*;
#[cfg(feature = "debugmozjs")]
use std::ptr;

#[derive(Debug)]
pub struct AutoRealm(JSAutoRealmAllowCCW);

impl AutoRealm {
    #[cfg(feature = "debugmozjs")]
    pub unsafe fn with_obj(cx: *mut JSContext,
                           target: *mut JSObject)
                           -> AutoRealm
    {
        let mut notifier = mozilla::detail::GuardObjectNotifier {
            mStatementDone: ptr::null_mut(),
        };

        AutoRealm(
            JSAutoRealmAllowCCW::new(
                cx,
                target,
                &mut notifier as *mut _))
    }

    #[cfg(not(feature = "debugmozjs"))]
    pub unsafe fn with_obj(cx: *mut JSContext,
                           target: *mut JSObject)
                           -> AutoRealm
    {
        AutoRealm(JSAutoRealmAllowCCW::new(cx, target))
    }

    #[cfg(feature = "debugmozjs")]
    pub unsafe fn with_script(cx: *mut JSContext,
                              target: *mut JSScript)
                              -> AutoRealm
    {
        let mut notifier = mozilla::detail::GuardObjectNotifier {
            mStatementDone: ptr::null_mut(),
        };

        AutoRealm(
            JSAutoRealmAllowCCW::new1(
                cx,
                target,
                &mut notifier as *mut _))
    }

    #[cfg(not(feature = "debugmozjs"))]
    pub unsafe fn with_script(cx: *mut JSContext,
                              target: *mut JSScript)
                              -> AutoRealm
    {
        AutoRealm(JSAutoRealmAllowCCW::new1(cx, target))
    }
}
