#############################################################################
# $Id: Utils.pm,v 1.2 1998/07/22 22:38:38 leif Exp $
#
# The contents of this file are subject to the Mozilla Public License
# Version 1.0 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS"
# basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
# License for the specific language governing rights and limitations
# under the License.
#
# The Original Code is PerlDap. The Initial Developer of the Original
# Code is Netscape Communications Corp. and Clayton Donley. Portions
# created by Netscape are Copyright (C) Netscape Communications
# Corp., portions created by Clayton Donley are Copyright (C) Clayton
# Donley. All Rights Reserved.
#
# Contributor(s):
#
# DESCRIPTION
#    Lots of Useful Little Utilities, for LDAP related operations.
#
#############################################################################

package Mozilla::LDAP::Utils;

require Mozilla::LDAP::API;


#############################################################################
# Normalize the DN string (first argument), and return the new, normalized,
# string.
#
sub normalizeDN
{
  my ($self, $dn) = @_;
  my (@vals);

  $dn = $self->{dn} unless $dn;
  return "" if ($dn eq "");

  $vals = Mozilla::LDAP::API::ldap_explode_dn(lc $dn, 0);

  return join(",", @{$vals});
}
