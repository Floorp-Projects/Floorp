MODULE = dom

REQUIRES += \
  string \
  xpcom \
  webbrwsr \
  commandhandler \
  js \
  widget \
  gfx \
  thebes \
  layout \
  content \
  caps \
  docshell \
  xpconnect \
  pref \
  oji \
  necko \
  nkcache \
  mimetype \
  java \
  locale \
  prefetch \
  xuldoc \
  webshell \
  view \
  uconv \
  shistory \
  plugin \
  windowwatcher \
  chardet \
  find \
  appshell \
  intl \
  unicharutil \
  rdf \
  xultmpl \
  jar \
  storage \
  htmlparser \
  $(NULL)

ifdef NS_TRACE_MALLOC
REQUIRES += tracemalloc
endif

ifdef MOZ_JSDEBUGGER
REQUIRES += jsdebug
endif

DOM_SRCDIRS = \
  dom/base \
  dom/src/events \
  dom/src/storage \
  dom/src/offline \
  dom/src/geolocation \
  dom/src/threads \
  content/xbl/src \
  content/xul/document/src \
  content/events/src \
  content/base/src \
  content/html/content/src \
  content/html/document/src \
  layout/generic \
  layout/style \
  layout/xul/base/src \
  layout/xul/base/src/tree/src \
  $(NULL)

LOCAL_INCLUDES += $(DOM_SRCDIRS:%=-I$(topsrcdir)/%)
DEFINES += -D_IMPL_NS_LAYOUT
