#ifndef BOOTANIMATION_H
#define BOOTANIMATION_H

namespace android {
class FramebufferNativeWindow;
}

/* This returns a FramebufferNativeWindow if one exists.
 * If not, one is created and the boot animation is started. */
__attribute__ ((weak))
android::FramebufferNativeWindow* NativeWindow();

/* This stops the boot animation if it's still running. */
__attribute__ ((weak))
void StopBootAnimation();

#endif /* BOOTANIMATION_H */
