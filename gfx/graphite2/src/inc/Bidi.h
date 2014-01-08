/*  GRAPHITE2 LICENSING

    Copyright 2013, SIL International
    All rights reserved.

    This library is free software; you can redistribute it and/or modify
    it under the terms of the GNU Lesser General Public License as published
    by the Free Software Foundation; either version 2.1 of License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should also have received a copy of the GNU Lesser General Public
    License along with this library in the file named "LICENSE".
    If not, write to the Free Software Foundation, 51 Franklin Street,
    Suite 500, Boston, MA 02110-1335, USA or visit their web page on the
    internet at http://www.fsf.org/licenses/lgpl.html.

Alternatively, the contents of this file may be used under the terms of the
Mozilla Public License (http://mozilla.org/MPL) or the GNU General Public
License, as published by the Free Software Foundation, either version 2
of the License or (at your option) any later version.
*/
#pragma once

namespace graphite2
{

class BracketPair
{
public:
    BracketPair(uint16 g, Slot *s, uint8 b, BracketPair *p, BracketPair *l) : _open(s), _close(0), _parent(p), _next(0), _prev(l), _gid(g), _mask(0), _before(b) {};
    uint16 gid() const { return _gid; }
    Slot *open() const { return _open; }
    Slot *close() const { return _close; }
    uint8 mask() const { return _mask; }
    int8 before() const { return _before; }
    BracketPair *parent() const { return _parent; }
    void close(Slot *s) { _close = s; }
    BracketPair *next() const { return _next; }
    BracketPair *prev() const { return _prev; }
    void next(BracketPair *n) { _next = n; }
    void orin(uint8 m) { _mask |= m; }
private:
    Slot        *   _open;          // Slot of opening paren
    Slot        *   _close;         // Slot of closing paren
    BracketPair *   _parent;        // pair this pair is in or NULL
    BracketPair *   _next;          // next pair along the string
    BracketPair *   _prev;          // pair that closed last before we opened
    uint16          _gid;           // gid of closing paren
    uint8           _mask;          // bitmap (2 bits) of directions within the pair
    int8            _before;        // most recent strong type (L, R, OPP, CPP)
};

class BracketPairStack
{
public:
    BracketPairStack(int s) : _stack(grzeroalloc<BracketPair>(s)), _ip(_stack - 1), _top(0), _last(0), _lastclose(0), _size(s) {}
    ~BracketPairStack() { free(_stack); }

public:
    BracketPair *scan(uint16 gid);
    void close(BracketPair *tos, Slot *s);
    BracketPair *push(uint16 gid, Slot *pos, uint8 before, int prevopen);
    void orin(uint8 mask);
    void clear() { _ip = _stack - 1; _top = 0; _last = 0; _lastclose = 0; }
    int size() const { return _size; }
    BracketPair *start() const { return _stack; }

    CLASS_NEW_DELETE

private:

    BracketPair *_stack;        // start of storage
    BracketPair *_ip;           // where to add the next pair
    BracketPair *_top;          // current parent
    BracketPair *_last;         // end of next() chain
    BracketPair *_lastclose;    // last pair to close
    int          _size;         // capacity
};

inline BracketPair *BracketPairStack::scan(uint16 gid)
{
    BracketPair *res = _top;
    while (res >= _stack)
    {
        if (res->gid() == gid)
            return res;
        res = res->parent();
    }
    return 0;
}

inline void BracketPairStack::close(BracketPair *tos, Slot *s) 
{
    for ( ; _last && _last != tos && !_last->close(); _last = _last->parent())
    { }
    tos->close(s);
    _last->next(NULL);
    _lastclose = tos;
    _top = tos->parent();
}

inline BracketPair *BracketPairStack::push(uint16 gid, Slot *pos, uint8 before, int prevopen)
{
    if (++_ip - _stack < _size && _stack)
    {
        ::new (_ip) BracketPair(gid, pos, before, _top, prevopen ? _last : _lastclose);
        if (_last) _last->next(_ip);
        _last = _ip;
    }
    _top = _ip;
    return _ip;
}

inline void BracketPairStack::orin(uint8 mask)
{
    BracketPair *t = _top;
    for ( ; t; t = t->parent())
        t->orin(mask);
}

}
