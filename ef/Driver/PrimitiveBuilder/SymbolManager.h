/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */

#ifndef _SYMBOL_MANAGER_H_
#define _SYMBOL_MANAGER_H_

#include "Fundamentals.h"
#include "HashTable.h"
#include "Vector.h"
#include "Pool.h"

template <class Owner, class Consumer>
struct SymbolStruct
{
  bool               defined;
  bool               used;
  char *             name;
  Owner *            owner;
  Vector<Consumer *> pending;

  SymbolStruct() : defined(0), used(0), name(0), owner(0) {}
};

template <class Owner, class Consumer>
class SymbolManager
{
protected:
  typedef SymbolStruct<Owner, Consumer> Symbol;   
  HashTable<Symbol>                     symbols;
  uint32                                n_undefined;
  Vector<char *>                        ret_vector;

public:

  SymbolManager(Pool& pool) : symbols(pool), n_undefined(0) {}

  void resolve(const char *name, Consumer& consumer)
    {
      if (!defined(name))
		differ(name, consumer);
      else
		link(*symbols[name].owner, consumer);
      symbols[name].used = 1;
    }

  Owner& get(const char *name)
	{
	  Symbol& symbol = symbols[name];
	  assert(symbol.defined);
	  return *symbol.owner;
	}

  virtual void link(Owner& owner, Consumer& consumer) = 0;

  void define(const char *name, Owner& owner)
    {
      if (symbols.exists(name))
		{
		  Symbol& symbol = symbols[name];
		  uint32 count = symbol.pending.size();
	  
		  assert(!symbol.defined );
		  symbol.owner = &owner;
		  symbol.defined = 1;
		  for (uint32 i = 0; i < count; i++)
			link(owner, *symbol.pending[i]);

		  n_undefined--;
		}
      else
		{
		  Symbol symbol;
		  symbol.owner = &owner;
		  symbol.defined = 1;
		  symbol.name = strdup(name);
		  symbols.add(name, symbol);
		}
    }

  inline bool exists(const char *name) {return symbols.exists(name);}
  inline bool defined(const char *name) {return exists(name) && symbols[name].defined;}
  inline bool used(const char *name) {return exists(name) && symbols[name].used;}

  void differ(const char *name, Consumer& consumer)
    {
      if (!exists(name))
		{
		  Symbol symbol;
		  symbol.name = strdup(name);
		  symbols.add(name, symbol);
		  n_undefined++;
		}
      symbols[name].pending.append(&consumer);
    }

  bool hasUndefinedSymbols() {return (n_undefined != 0);}

  Vector<char *>& undefinedSymbols() 
    {
      Vector<Symbol>& all_symbols = symbols;
      uint32 count= all_symbols.size();

      ret_vector.eraseAll();
      
      if (n_undefined)
		for (uint32 i = 0; i < count; i++)
		  if (!all_symbols[i].defined)
			ret_vector.append(all_symbols[i].name);

      return ret_vector;
    }

  bool hasUnusedSymbols() 
    {
      Vector<Symbol>& all_symbols = symbols;
      uint32 count= all_symbols.size();
      for (uint32 i = 0; i < count; i++)
		if (!all_symbols[i].used)
		  return 1;
      return 0;
    }

  Vector<char *>& unusedSymbols() 
    {
      Vector<Symbol>& all_symbols = symbols;
      uint32 count= all_symbols.size();

      ret_vector.eraseAll();
      
      for (uint32 i = 0; i < count; i++)
		if (!all_symbols[i].used)
		  ret_vector.append(all_symbols[i].name);
      
      return ret_vector;
    }
};
 
#endif /* _SYMBOL_MANAGER_H_ */
