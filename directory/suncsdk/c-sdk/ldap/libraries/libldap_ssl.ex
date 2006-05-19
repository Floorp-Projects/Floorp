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
450	ldapssl_client_init
451	ldapssl_init
452	ldapssl_install_routines
453	ldapssl_clientauth_init
454	ldapssl_enable_clientauth
456	ldapssl_advclientauth_init
457	ldapssl_pkcs_init
458	ldapssl_err2string
459	ldapssl_serverauth_init
460	ldapssl_tls_start_s
461	ldapssl_set_strength
462 ldapssl_set_option
463 ldapssl_get_option
464 ldap_start_tls_s
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

#

# the last Windows ordinal number that has been reserved for SSL is 469.

# Windows ordinals 1100-1150 are reserved for privately/non-published
# exported routines
