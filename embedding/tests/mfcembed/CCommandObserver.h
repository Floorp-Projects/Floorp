#pragma once

#include "nsIObserver.h"

class CFrameWnd;


class CCommandObserver : public nsIObserver
{
public:
  NS_DECL_ISUPPORTS

  CCommandObserver();
  ~CCommandObserver(){}
//NSIOBSERVER
  NS_DECL_NSIOBSERVER
//CCommandObserver
  void SetFrame(CFrameWnd *frame,UINT timerId,UINT delay); //update if 100 ticks goes by and no more changes
private:
  CFrameWnd *mFrame;
  UINT mDelay;
  UINT mTimerId;
};

