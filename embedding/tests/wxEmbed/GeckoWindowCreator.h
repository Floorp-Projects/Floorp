#ifndef GECKOWINDOWCREATOR_H
#define GECKOWINDOWCREATOR_H

#include "nsIWindowCreator.h"

class GeckoWindowCreator :
      public nsIWindowCreator
{
public:
    GeckoWindowCreator();
    virtual ~GeckoWindowCreator();

    NS_DECL_ISUPPORTS
    NS_DECL_NSIWINDOWCREATOR
};

#endif