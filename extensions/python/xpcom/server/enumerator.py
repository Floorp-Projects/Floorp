# The contents of this file are subject to the Mozilla Public License Version
# 1.1 (the "License"); you may not use this file except in compliance with the
# License. You may obtain a copy of the License at http://www.mozilla.org/MPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License for
# the specific language governing rights and limitations under the License.
#
# The Original Code is the Python XPCOM language bindings.
#
# The Initial Developer of the Original Code is ActiveState Tool Corp.
# Portions created by ActiveState Tool Corp. are Copyright (C) 2000, 2001
# ActiveState Tool Corp.  All Rights Reserved.
#
# Contributor(s): Mark Hammond <MarkH@ActiveState.com> (original author)
#

from xpcom import components

# This class is created by Python components when it
# needs to return an enumerator.
# For example, a component may implement a function:
#  nsISimpleEnumerator enumSomething();
# This could could simply say:
# return SimpleEnumerator([something1, something2, something3])
class SimpleEnumerator:
    _com_interfaces_ = [components.interfaces.nsISimpleEnumerator]

    def __init__(self, data):
        self._data = data
        self._index = 0

    def hasMoreElements(self):
        return self._index < len(self._data)
    
    def getNext(self):
        self._index = self._index + 1
        return self._data[self._index-1]
