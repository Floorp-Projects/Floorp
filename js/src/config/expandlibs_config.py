# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0. If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.

from buildconfig import substs

def normalize_suffix(suffix):
    '''Returns a normalized suffix, i.e. ensures it starts with a dot and
    doesn't starts or ends with whitespace characters'''
    value = suffix.strip()
    if len(value) and not value.startswith('.'):
        value = '.' + value
    return value

# Variables from the build system
AR = substs['AR']
AR_EXTRACT = substs['AR_EXTRACT'].replace('$(AR)', AR)
DLL_PREFIX = substs['DLL_PREFIX']
LIB_PREFIX = substs['LIB_PREFIX']
OBJ_SUFFIX = normalize_suffix(substs['OBJ_SUFFIX'])
LIB_SUFFIX = normalize_suffix(substs['LIB_SUFFIX'])
DLL_SUFFIX = normalize_suffix(substs['DLL_SUFFIX'])
IMPORT_LIB_SUFFIX = normalize_suffix(substs['IMPORT_LIB_SUFFIX'])
LIBS_DESC_SUFFIX = normalize_suffix(substs['LIBS_DESC_SUFFIX'])
EXPAND_LIBS_LIST_STYLE = substs['EXPAND_LIBS_LIST_STYLE']
EXPAND_LIBS_ORDER_STYLE = substs['EXPAND_LIBS_ORDER_STYLE']
LD_PRINT_ICF_SECTIONS = substs['LD_PRINT_ICF_SECTIONS']
