# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with
# the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is
# ActiveState Tool Corp.
# Portions created by the Initial Developer are Copyright (C) 2000, 2001
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#   Mark Hammond <mhammond@skippinet.com.au> (original author)
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****

# Could maybe later have a process that extracted these enums should they change.
# from nsFileLocations.h
App_DirectoryBase              = 0x00010000
App_PrefsDirectory30           = App_DirectoryBase +    1 
App_PrefsDirectory40           = App_DirectoryBase +    2
App_PrefsDirectory50           = App_DirectoryBase +    3
App_ResDirectory               = App_DirectoryBase +    5
App_UserProfileDirectory30     = App_DirectoryBase +   10
App_UserProfileDirectory40     = App_DirectoryBase +   11
App_UserProfileDirectory50     = App_DirectoryBase +   12
App_DefaultUserProfileRoot30   = App_DirectoryBase +   13
App_DefaultUserProfileRoot40   = App_DirectoryBase +   14
App_DefaultUserProfileRoot50   = App_DirectoryBase +   15
App_ProfileDefaultsFolder30    = App_DirectoryBase +   16
App_ProfileDefaultsFolder40    = App_DirectoryBase +   17
App_ProfileDefaultsFolder50    = App_DirectoryBase +   18
App_PrefDefaultsFolder50       = App_DirectoryBase +   19
App_DefaultsFolder50           = App_DirectoryBase +   25
App_ComponentsDirectory        = App_DirectoryBase +   30
App_ChromeDirectory            = App_DirectoryBase +   31
App_PluginsDirectory           = App_DirectoryBase +   32
App_UserChromeDirectory        = App_DirectoryBase +   40
App_FileBase                   = App_DirectoryBase + 1000
App_PreferencesFile30          = App_FileBase      +    1
App_PreferencesFile40          = App_FileBase      +    2
App_PreferencesFile50          = App_FileBase      +    3
App_BookmarksFile30            = App_FileBase      +   10
App_BookmarksFile40            = App_FileBase      +   11
App_BookmarksFile50            = App_FileBase      +   12
App_Registry40                 = App_FileBase      +   20
App_Registry50                 = App_FileBase      +   21
App_LocalStore50               = App_FileBase   +  30
App_History50              = App_FileBase   +  40
App_MailDirectory50        = App_FileBase   +  50
App_ImapMailDirectory50    = App_FileBase   +  60
App_NewsDirectory50        = App_FileBase   +  70
App_MessengerFolderCache50 = App_FileBase   +  80
App_UsersPanels50          = App_FileBase   +  90
App_SearchFile50           = App_FileBase   + 100
App_SearchDirectory50      = App_FileBase   + 101

# From nsSpecialSystemDirectory.h
OS_DriveDirectory         =   1
OS_TemporaryDirectory     =   2
OS_CurrentProcessDirectory=   3
OS_CurrentWorkingDirectory=   4

XPCOM_CurrentProcessComponentDirectory=   5
XPCOM_CurrentProcessComponentRegistry=   6            

Moz_BinDirectory          = 10

Mac_SystemDirectory       =   101
Mac_DesktopDirectory      =   102
Mac_TrashDirectory        =   103
Mac_StartupDirectory      =   104
Mac_ShutdownDirectory     =   105
Mac_AppleMenuDirectory    =   106
Mac_ControlPanelDirectory =   107
Mac_ExtensionDirectory    =   108
Mac_FontsDirectory        =   109
Mac_PreferencesDirectory  =   110
Mac_DocumentsDirectory    =   111
Mac_InternetSearchDirectory    =   112

Win_SystemDirectory       =   201
Win_WindowsDirectory      =   202

Win_HomeDirectory         =   203
Win_Desktop               =   204    
Win_Programs              =   205    
Win_Controls              =   206    
Win_Printers              =   207    
Win_Personal              =   208    
Win_Favorites             =   209    
Win_Startup               =   210    
Win_Recent                =   211    
Win_Sendto                =   212    
Win_Bitbucket             =   213    
Win_Startmenu             =   214    
Win_Desktopdirectory      =   215    
Win_Drives                =   216    
Win_Network               =   217    
Win_Nethood               =   218    
Win_Fonts                 =   219    
Win_Templates             =   220    
Win_Common_Startmenu      =   221    
Win_Common_Programs       =   222    
Win_Common_Startup        =   223   
Win_Common_Desktopdirectory = 224   
Win_Appdata               =   225    
Win_Printhood             =   226    

Unix_LocalDirectory       =   301
Unix_LibDirectory         =   302
Unix_HomeDirectory        =   303

BeOS_SettingsDirectory    =   401
BeOS_HomeDirectory        =   402
BeOS_DesktopDirectory     =   403
BeOS_SystemDirectory      =   404

OS2_SystemDirectory        =   501

# Type/Variant related constants.
TD_INT8              = 0
TD_INT16             = 1
TD_INT32             = 2
TD_INT64             = 3
TD_UINT8             = 4
TD_UINT16            = 5
TD_UINT32            = 6
TD_UINT64            = 7
TD_FLOAT             = 8
TD_DOUBLE            = 9
TD_BOOL              = 10
TD_CHAR              = 11 
TD_WCHAR             = 12
TD_VOID              = 13  
TD_PNSIID            = 14
TD_DOMSTRING     = 15
TD_PSTRING           = 16
TD_PWSTRING          = 17
TD_INTERFACE_TYPE    = 18
TD_INTERFACE_IS_TYPE = 19
TD_ARRAY             = 20
TD_PSTRING_SIZE_IS   = 21
TD_PWSTRING_SIZE_IS  = 22
TD_UTF8STRING        = 23
TD_CSTRING           = 24
TD_ASTRING           = 25

# From xpt_struct.h
XPT_TDP_POINTER          = 0x80
XPT_TDP_UNIQUE_POINTER   = 0x40
XPT_TDP_REFERENCE        = 0x20
XPT_TDP_FLAGMASK         = 0xe0
XPT_TDP_TAGMASK          = (~XPT_TDP_FLAGMASK)
def XPT_TDP_TAG(tdp): return (tdp & XPT_TDP_TAGMASK)

def XPT_TDP_IS_POINTER(flags): return (flags & XPT_TDP_POINTER)
def XPT_TDP_IS_UNIQUE_POINTER(flags): return (flags & XPT_TDP_UNIQUE_POINTER)
def XPT_TDP_IS_REFERENCE(flags): return (flags & XPT_TDP_REFERENCE)

XPT_ID_SCRIPTABLE           = 0x80
XPT_ID_FLAGMASK             = 0x80
XPT_ID_TAGMASK              = ~XPT_ID_FLAGMASK
def XPT_ID_TAG(id): return id & XPT_ID_TAGMASK

def XPT_ID_IS_SCRIPTABLE(flags): return flags & XPT_ID_SCRIPTABLE

XPT_PD_IN       = 0x80
XPT_PD_OUT      = 0x40
XPT_PD_RETVAL   = 0x20
XPT_PD_SHARED   = 0x10
XPT_PD_DIPPER   = 0x08
XPT_PD_FLAGMASK = 0xf0

def XPT_PD_IS_IN(flags): return (flags & XPT_PD_IN)
def XPT_PD_IS_OUT(flags): return (flags & XPT_PD_OUT)
def XPT_PD_IS_RETVAL(flags): return (flags & XPT_PD_RETVAL)
def XPT_PD_IS_SHARED(flags): return (flags & XPT_PD_SHARED)
def XPT_PD_IS_DIPPER(flags): return (flags & XPT_PD_DIPPER)

XPT_MD_GETTER = 0x80
XPT_MD_SETTER = 0x40
XPT_MD_NOTXPCOM = 0x20
XPT_MD_CTOR = 0x10
XPT_MD_HIDDEN = 0x08
XPT_MD_FLAGMASK = 0xf8

def XPT_MD_IS_GETTER(flags):     return (flags & XPT_MD_GETTER)
def XPT_MD_IS_SETTER(flags):     return (flags & XPT_MD_SETTER)
def XPT_MD_IS_NOTXPCOM(flags):   return (flags & XPT_MD_NOTXPCOM)
def XPT_MD_IS_CTOR(flags):       return (flags & XPT_MD_CTOR)
def XPT_MD_IS_HIDDEN(flags):     return (flags & XPT_MD_HIDDEN)

# From xptinfo.h

T_I8                = TD_INT8
T_I16               = TD_INT16
T_I32               = TD_INT32
T_I64               = TD_INT64
T_U8                = TD_UINT8
T_U16               = TD_UINT16
T_U32               = TD_UINT32
T_U64               = TD_UINT64
T_FLOAT             = TD_FLOAT
T_DOUBLE            = TD_DOUBLE
T_BOOL              = TD_BOOL
T_CHAR              = TD_CHAR
T_WCHAR             = TD_WCHAR
T_VOID              = TD_VOID
T_IID               = TD_PNSIID
T_DOMSTRING        = TD_DOMSTRING
T_CHAR_STR          = TD_PSTRING
T_WCHAR_STR         = TD_PWSTRING
T_INTERFACE         = TD_INTERFACE_TYPE
T_INTERFACE_IS      = TD_INTERFACE_IS_TYPE
T_ARRAY             = TD_ARRAY
T_PSTRING_SIZE_IS   = TD_PSTRING_SIZE_IS
T_PWSTRING_SIZE_IS  = TD_PWSTRING_SIZE_IS
T_UTF8STRING        = TD_UTF8STRING
T_CSTRING           = TD_CSTRING
T_ASTRING           = TD_ASTRING

# from nsIVariant
VTYPE_INT8 = 0
VTYPE_INT16 = 1
VTYPE_INT32 = 2
VTYPE_INT64 = 3
VTYPE_UINT8 = 4
VTYPE_UINT16 = 5
VTYPE_UINT32 = 6
VTYPE_UINT64 = 7
VTYPE_FLOAT = 8
VTYPE_DOUBLE = 9
VTYPE_BOOL = 10
VTYPE_CHAR = 11
VTYPE_WCHAR = 12
VTYPE_VOID = 13
VTYPE_ID = 14
VTYPE_DOMSTRING = 15
VTYPE_CHAR_STR = 16
VTYPE_WCHAR_STR = 17
VTYPE_INTERFACE = 18
VTYPE_INTERFACE_IS = 19
VTYPE_ARRAY = 20
VTYPE_STRING_SIZE_IS = 21
VTYPE_WSTRING_SIZE_IS = 22
VTYPE_UTF8STRING = 23
VTYPE_CSTRING = 24
VTYPE_ASTRING = 25
VTYPE_EMPTY_ARRAY = 254
VTYPE_EMPTY = 255
