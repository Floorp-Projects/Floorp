# Profiler Popup

This directory controls the creation of a popup widget that can be used to record performance profiles. It is slightly different than the rest of the DevTools code, as it can be loaded independently of the rest of DevTools. The instrumentation from DevTools adds significant overhead to profiles, so this recording popup (and its shortcuts) enable a low-overhead profiling experience. This button can be enabled via the Tools -> Web Developer menu.
