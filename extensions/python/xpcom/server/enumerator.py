# Copyright (c) 2000-2001 ActiveState Tool Corporation.
# See the file LICENSE.txt for licensing information.

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
