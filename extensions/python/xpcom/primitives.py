# Various utilities for working with nsISupportsPrimitive
from xpcom import components

_primitives_map = {}

def _build_map():
    ifaces = components.interfaces
    iface = ifaces.nsISupportsPrimitive
    m = _primitives_map
    
    m[iface.TYPE_ID] = ifaces.nsISupportsID
    m[iface.TYPE_CSTRING] = ifaces.nsISupportsCString
    m[iface.TYPE_STRING] = ifaces.nsISupportsString
    m[iface.TYPE_PRBOOL] = ifaces.nsISupportsPRBool
    m[iface.TYPE_PRUINT8] = ifaces.nsISupportsPRUint8
    m[iface.TYPE_PRUINT16] = ifaces.nsISupportsPRUint16
    m[iface.TYPE_PRUINT32] = ifaces.nsISupportsPRUint32
    m[iface.TYPE_PRUINT64] = ifaces.nsISupportsPRUint64
    m[iface.TYPE_PRINT16] = ifaces.nsISupportsPRInt16
    m[iface.TYPE_PRINT32] = ifaces.nsISupportsPRInt32
    m[iface.TYPE_PRINT64] = ifaces.nsISupportsPRInt64
    m[iface.TYPE_PRTIME] = ifaces.nsISupportsPRTime
    m[iface.TYPE_CHAR] = ifaces.nsISupportsChar
    m[iface.TYPE_FLOAT] = ifaces.nsISupportsFloat
    m[iface.TYPE_DOUBLE] = ifaces.nsISupportsDouble
    # Do interface pointer specially - it provides the IID.
    #m[iface.TYPE_INTERFACE_POINTER] = ifaces.nsISupportsDouble
    
def GetPrimitive(ob):
    if len(_primitives_map)==0:
        _build_map()

    prin = ob.QueryInterface(components.interfaces.nsISupportsPrimitive)
    try:
        better = _primitives_map[prin.type]
    except KeyError:
        raise ValueError, "This primitive type (%d) is not supported" % (prin.type,)
    prin = prin.QueryInterface(better)
    return prin.data
