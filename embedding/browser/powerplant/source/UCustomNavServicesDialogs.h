
#pragma once

#include "UNavServicesDialogs.h"


namespace UNavServicesDialogs {


	pascal void	NavCustomEventProc(
						NavEventCallbackMessage	inSelector,
						NavCBRecPtr					    ioParams,
						NavCallBackUserData			ioUserData);


  class LCustomFileDesignator : public LFileDesignator
  {
    public:
    						LCustomFileDesignator();
    						~LCustomFileDesignator();

  	bool				AskDesignateFile(ConstStringPtr inDefaultName, ESaveFormat& ioSaveFormat);

  };

}
