DOM_SRCDIRS = \
  dom/base \
  dom/battery \
  dom/sms/src \
  dom/src/events \
  dom/src/storage \
  dom/src/offline \
  dom/src/geolocation \
  dom/src/notification \
  dom/telephony \
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

LOCAL_INCLUDES += $(DOM_SRCDIRS:%=-I$(topsrcdir)/%)
DEFINES += -D_IMPL_NS_LAYOUT
