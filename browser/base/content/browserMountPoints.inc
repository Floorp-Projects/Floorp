<stringbundleset id="stringbundleset"/>

<commandset id="mainCommandSet"/>
<commandset id="baseMenuCommandSet"/>
#ifdef MOZ_PLACES_BOOKMARKS
<commandset id="placesCommands"/>
#endif

<broadcasterset id="mainBroadcasterSet"/>

<keyset id="mainKeyset"/>
<keyset id="baseMenuKeyset"/>

<menubar id="main-menubar"/>

#ifndef MOZ_PLACES_BOOKMARKS
<template id="bookmarks-template"/>
#endif

#ifndef MOZ_PLACES
<tree id="hiddenHistoryTree"/>
#endif
