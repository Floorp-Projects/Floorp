#ifndef _BASEAFF_HXX_
#define _BASEAFF_HXX_

class AffEntry
{
protected:
       char *       appnd;
       char *       strip;
       short        appndl;
       short        stripl;
       short        numconds;
       short        xpflg;
       char         achar;
       char         conds[SETSIZE];
};

#endif
