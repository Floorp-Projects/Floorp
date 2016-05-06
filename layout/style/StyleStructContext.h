/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_StyleStructContext_h
#define mozilla_StyleStructContext_h

#include "CounterStyleManager.h"
#include "mozilla/LookAndFeel.h"
#include "nsPresContext.h"

class nsDeviceContext;

/**
 * Construction context for style structs.
 *
 * Certain Gecko style structs have historically taken an nsPresContext
 * argument in their constructor, which is used to compute various things. This
 * makes Gecko style structs document-specific (which Servo style structs are
 * not), and means that the initial values for style-structs cannot be shared
 * across the entire runtime (as is the case in Servo).
 *
 * We'd like to remove this constraint so that Gecko can get the benefits of the
 * Servo model, and so that Gecko aligns better with the Servo style system when
 * using it. Unfortunately, this may require a fair amount of work, especially
 * related to text zoom.
 *
 * So as an intermediate step, we define a reduced API set of "things that are
 * needed when constructing style structs". This just wraps and forwards to an
 * nsPresContext in the Gecko case, and returns some default not-always-correct
 * values in the Servo case. We can then focus on reducing this API surface to
 * zero, at which point this can be removed.
 *
 * We don't put the type in namespace mozilla, since we expect it to be
 * temporary, and the namespacing would clutter up nsStyleStruct.h.
 */
#ifdef MOZ_STYLO
#define SERVO_DEFAULT(default_val) { if (!mPresContext) { return default_val; } }
#else
#define SERVO_DEFAULT(default_val) { MOZ_ASSERT(mPresContext); }
#endif
class StyleStructContext {
public:
  MOZ_IMPLICIT StyleStructContext(nsPresContext* aPresContext)
    : mPresContext(aPresContext) { MOZ_ASSERT(aPresContext); }
  static StyleStructContext ServoContext() { return StyleStructContext(); }

  // XXXbholley: Need to make text zoom work with stylo. This probably means
  // moving the zoom handling out of computed values and into a post-
  // computation.
  float TextZoom()
  {
    SERVO_DEFAULT(1.0);
    return mPresContext->TextZoom();
  }

  const nsFont* GetDefaultFont(uint8_t aFontID)
  {
    // NB: The servo case only differ from the default case in terms of which
    // font pref cache gets used. The distinction is probably unnecessary.
    SERVO_DEFAULT(mozilla::StaticPresData::Get()->
                  GetDefaultFont(aFontID, GetLanguageFromCharset()));
    return mPresContext->GetDefaultFont(aFontID, GetLanguageFromCharset());
  }

  uint32_t GetBidi()
  {
    SERVO_DEFAULT(0);
    return mPresContext->GetBidi();
  }

  int32_t AppUnitsPerDevPixel();

  nscoord DevPixelsToAppUnits(int32_t aPixels)
  {
    return NSIntPixelsToAppUnits(aPixels, AppUnitsPerDevPixel());
  }

  typedef mozilla::LookAndFeel LookAndFeel;
  nscolor DefaultColor()
  {
    SERVO_DEFAULT(LookAndFeel::GetColor(LookAndFeel::eColorID_WindowForeground,
                                        NS_RGB(0x00, 0x00, 0x00)));
    return mPresContext->DefaultColor();
  }

  mozilla::CounterStyle* BuildCounterStyle(const nsSubstring& aName)
  {
    SERVO_DEFAULT(mozilla::CounterStyleManager::GetBuiltinStyle(NS_STYLE_LIST_STYLE_DISC));
    return mPresContext->CounterStyleManager()->BuildCounterStyle(aName);
  }

  nsIAtom* GetLanguageFromCharset() const
  {
    SERVO_DEFAULT(nsGkAtoms::x_western);
    return mPresContext->GetLanguageFromCharset();
  }

  already_AddRefed<nsIAtom> GetContentLanguage() const
  {
    SERVO_DEFAULT(do_AddRef(nsGkAtoms::x_western));
    return mPresContext->GetContentLanguage();
  }

private:
  nsDeviceContext* DeviceContext()
  {
    SERVO_DEFAULT(HackilyFindSomeDeviceContext());
    return mPresContext->DeviceContext();
  }

  nsDeviceContext* HackilyFindSomeDeviceContext();

  StyleStructContext() : mPresContext(nullptr) {}
  nsPresContext* mPresContext;
};

#undef SERVO_DEFAULT

#endif // mozilla_StyleStructContext_h
