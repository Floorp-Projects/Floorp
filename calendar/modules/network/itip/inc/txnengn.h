// txnengn.h
// John Sun
// 4:24 PM March 9, 1998

#ifndef __TRANSACTIONENGINE_H_
#define __TRANSCATIONENGINE_H_

#include "txnobj.h"

class TransactionEngine
{
private:
    CommunicationObject * m_CommObj;

public:
    TransactionEngine();

    TransactionEngine(CommunicationObject * initCommObj);

    ~TransactionEngine();
    
    XPPtrArray * execute();
    
};
#endif