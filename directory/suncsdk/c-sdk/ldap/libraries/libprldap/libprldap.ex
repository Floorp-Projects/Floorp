# 
# The contents of this file are subject to the Mozilla Public
# License Version 1.1 (the "License"); you may not use this file
# except in compliance with the License. You may obtain a copy of
# the License at http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS
# IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
# implied. See the License for the specific language governing
# rights and limitations under the License.
# 
# The Original Code is Mozilla Communicator client code.
# 
# The Initial Developer of the Original Code is Netscape
# Communications Corporation.  Portions created by Netscape are 
# Copyright (C) 1998-2000 Netscape Communications Corporation.  All
# Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the
# terms of the GNU General Public License Version 2 or later (the
# "GPL"), in which case the provisions of the GPL are applicable 
# instead of those above.  If you wish to allow use of your 
# version of this file only under the terms of the GPL and not to
# allow others to use your version of this file under the MPL,
# indicate your decision by deleting the provisions above and
# replace them with the notice and other provisions required by
# the GPL.  If you do not delete the provisions above, a recipient
# may use your version of this file under either the MPL or the
# GPL.
# 

#<SunOS>LDAPCSDK_5.10 {        # LDAP C SDK 5.10 release
#<SunOS>   global:
2010	prldap_init
2011	prldap_install_routines
2012	prldap_set_session_info
2013	prldap_get_session_info
2014	prldap_set_socket_info
2015	prldap_get_socket_info
2016	prldap_set_session_option
2017	prldap_get_session_option
2018	prldap_set_default_socket_info
2019	prldap_get_default_socket_info
2020	prldap_is_installed
2021	prldap_import_connection
#<SunOS>
#<SunOS>   local:
#<SunOS>	*;
#<SunOS>};
#
#
#   When new interfaces are added in the future, it must embed the
#   version information to the comments for the SunOS (SOLARIS)
#   package, such as:
#       #<SunOS>LDAPCSDK_6_21 {
#       #<SunOS>   global:
#       new interfaces here...
#       new interfaces here...
#       new interfaces here...
#       #<SunOS>
#       #<SunOS>   local:
#       #<SunOS>	*;
#       #<SunOS>} LDAPCSDK_5_10;
