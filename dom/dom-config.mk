DOM_SRCDIRS = \
  dom/base \
  dom/battery \
  dom/power \
  dom/network/src \
  dom/settings \
  dom/sms/src \
  dom/contacts \
  dom/src/events \
  dom/src/storage \
  dom/src/offline \
  dom/src/geolocation \
  dom/src/notification \
  dom/workers \
  content/xbl/src \
  content/xul/document/src \
  content/events/src \
  content/base/src \
  content/html/content/src \
  content/html/document/src \
  content/svg/content/src \
  layout/generic \
  layout/style \
  layout/xul/base/src \
  layout/xul/base/src/tree/src \
  $(NULL)

ifdef MOZ_B2G_RIL
DOM_SRCDIRS += \
  dom/system/gonk \
  dom/telephony \
  dom/wifi \
  $(NULL)
endif

ifdef MOZ_B2G_BT
DOM_SRCDIRS += dom/bluetooth
endif

LOCAL_INCLUDES += $(DOM_SRCDIRS:%=-I$(topsrcdir)/%)
DEFINES += -D_IMPL_NS_LAYOUT
