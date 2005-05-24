#filter emptyLines

# This file is in the UTF-8 encoding

# Installer keys (don't translate strings in $$)
#define SETUP_TITLE $ProductShortName$ Setup
#define SETUP_WELCOME Welcome to %s
#define SETUP_MESSAGE0 You are about to install %s.
#define SETUP_MESSAGE1 It is strongly recommended that you exit all Windows programs before running this Setup program.
#define SETUP_MESSAGE1_UNIX It is strongly recommended that you exit all instances of %s before running this Setup program.
#define SETUP_MESSAGE2 Click Cancel to quit Setup and then close any programs you have running.  Click Next to continue the Setup program.
#define SETUP_MESSAGE3 Click Next to continue installing %s.

#define LICENSE_TITLE Software License Agreement
#define LICENSE_SUBTITLE Terms and conditions for using this software.
#define LICENSE_MESSAGE0 Please read the following license agreement. Use the scroll bar to view the rest of this agreement.
#define LICENSE_ACCEPT I &Accept the terms of the License Agreement
#define LICENSE_DECLINE I do NO&T Accept the terms of the License Agreement

#define SETUP_TYPE_TITLE Setup Type
#define SETUP_TYPE_SUBTITLE Choose setup options.
#define SETUP_TYPE_MESSAGE0 Choose the type of Setup you prefer, then click Next.
#define SETUP_TYPE_EXISTING The selected directory already contains a Firefox installation. To delete %s completely, please press the 'Delete Directory' button. Your Firefox settings should not be affected. Or, please press the 'Cancel' button and choose a different destination directory.

# &Ampersand is used to select a shortcut key
#define TYPE_STANDARD &Standard
#define TYPE_STANDARD_DESC Browser will be installed with the most common options.

#define TYPE_CUSTOM &Custom
#define TYPE_CUSTOM_DESC You may choose individual options to be installed. Recommended for experienced users.

#define SELECT_TITLE Select Components
#define SELECT_SUBTITLE Additional components that enhance $ProductShortName$.
#define SELECT_MESSAGE0 Choose additional components you want to install, then click Next.

#define LOCATION_TITLE Install Folder
#define LOCATION_SUBTITLE Where $ProductShortName$ will be installed.
#define LOCATION_MESSAGE0 $ProductShortName$ will be installed into the following folder:

#define UPGRADE_TITLE Upgrade
#define UPGRADE_SUBTITLE Upgrades an existing $ProductShortName$ installation.
#define UPGRADE_CLEANUP A previous $ProductNameInternal$ installation has been found in the chosen folder.

#define UPGRADE_CLEAN Perform a Clean Install
#define UPGRADE_CLEAN_MSG A Clean Install will COMPLETELY REMOVE the contents of the install folder! Any third party components you have installed will be lost. This is recommended as it may prevent incompatibilities. (XXXben)
#define UPGRADE_OVER If you choose not to perform a Clean Install, third party components will not be removed, which may cause unpredictable behavior.
#define UPGRADE_OVER_WINDIR A Safe Upgrade of $ProductNameInternal$ cannot be performed because the program is installed within the Windows folder. It is recommended that you choose another folder to install $ProductNameInternal$ into.

#define ADD_TITLE $ProductShortName$ Setup - Select Additional Components
#define ADD_MESSAGE0 Select or clear the additional components you want to install.

#define WININT_TITLE Set Up Shortcuts
#define WININT_SUBTITLE Creates Program Icons
#define WININT_MESSAGE0 Create icons for $ProductShortName$:

#define WININT_DESKTOP On my Desktop
#define WININT_START In my Start Menu Programs folder
#define WININT_QUICKL In my Quick Launch bar

#define USE_AS_DEFAULT Use %s as my Default Browser.

#define WININT_PFOLDER_TITLE $ProductShortName$ Setup - Select Program Folder
#define WININT_PFOLDER_MESSAGE0 Setup will add program icons to the Program Folder listed below.  You may type a new folder name, or select one from the Existing Folders list.  Click Next to continue.

#define ADDL_OPTIONS_TITLE $ProductShortName$ Setup - Additional Options
#define ADDL_OPTIONS_MSG1 Select the following option if you wish to save downloaded Setup files on your own computer.  This allows you to re-run Setup without downloading the files again.  The Setup files will be saved to the path below.

#define ADV_SETTINGS_TITLE $ProductShortName$ Setup - Advanced Settings
#define ADV_SETTINGS_MSG If your Internet connection requires a proxy server, enter your server name and port information.  If your Internet connection doesn't require a proxy server, leave the fields blank.

#define START_TITLE Summary
#define START_SUBTITLE Ready to start installing $ProductShortName$.
#define START_INSTALL Setup will now install the following components:
#define START_DOWNLOAD Setup will now download and install the following components:
#define START_MESSAGE0 Click Next to continue.

#define DL_TITLE Downloading
#define DL_SUBTITLE Downloading required components...
#define DL_BLURB Setup is downloading the files required to install $ProductShortName$.
#define DL_FILENAME Currently Downloading:
#define DL_TIMELEFT Time Remaining:

#define INSTALL_TITLE Installing
#define INSTALL_SUBTITLE Installing $ProductShortName$...
#define INSTALL_BLURB Setup is installing application files.
#define INSTALL_STATUSFILE Preparing File:
#define INSTALL_STATUSCOMP Currently Installing:

#define COMPLETE_TITLE Install Complete
#define COMPLETE_MESSAGE0 %s was successfully installed.
#define COMPLETE_MESSAGE1 Click Finish to complete Setup.
#define COMPLETE_LAUNCH Launch %s now.

#define DL2_TITLE $ProductShortName$ Setup - Download
#define DL2_MESSAGE0 Setup is downloading the required files now.  This may take some time depending on your network connection speed.
#define DL2_RETRY Some files have failed the CRC check and are being downloaded again.  Only those files that have failed will be downloaded.

#define CLOSE_CHECK Download of $ProductShortName$ was successful. $ProductNameInternal$ must be closed to proceed with installation. Click OK to exit $ProductNameInternal$ automatically and to begin installation.
#define CLOSE_CHECK2 $ProductNameInternal$ must be closed to proceed with installation. Click OK to exit $ProductNameInternal$ automatically and to begin installation.
#define CLOSE_WAIT Shutting down $ProductNameInternal$.  Please wait...

#define XPCOM_SHORT Cross-Platform COM
#define BROWSER_SHORT $ProductShortName$ Browser
#define HELP_SHORT $ProductShortName$ Help
#define UNINSTALL_SHORT $CompanyName$ Uninstaller
#define LANGPACK_TITLE Language Pack

#define ADT_SHORT Developer Tools
#define ADT_LONG The Document Inspector - a tool for web developers.

#define QFA_SHORT Quality Feedback Agent
#define QFA_LONG for reporting $ProductShortName$ crash information

#define RPT_SHORT Website Reporter
#define RPT_LONG for reporting $ProductShortName$ website compatibility problems

#define CORE_PREPARING Preparing Install, please wait...
#define INST_UNINST Installing the uninstaller, please wait...

#define MSG_UNFINISHED_DL A previous session of Setup did not finish downloading all the necessary files.  Would you like to use the files previously downloaded, to avoid downloading them again?
#define MSG_UNFINISHED_INSTALL A previous session of Setup did not finish with installation of all the necessary files.  Would you like to use the files previously downloaded, to avoid downloading them again?
#define MSG_CORRUPTED Setup has detected that the following archives within this Setup are corrupted:%sYou will need to obtain another copy of the $ProductShortName$ Setup in order for installation to proceed.
#define MSG_CORRUPTED_AUTO Setup has detected that at least one archive within this Setup is corrupt.  You will need to obtain another copy of the $ProductShortName$ Setup in order for installation to proceed.
#define MSG_TOO_MANY_CRC Too many download failures.  Setup will cancel.  Any files already downloaded will not be deleted.  The next time Setup is run, these files will be taken into account during the download.
#define MSG_TOO_MANY_NETWORK Too many network errors trying to download %s.  Setup will now Pause.  Please click Resume to retry downloading the files.
#define MSG_VERIFYING Verifying integrity of archives, please wait...
#define MSG_UNDEFINED Error undefined
#define MSG_OOM Out of memory!
#define MSG_STATUS_DL %s at %.2f KB/sec (%u KB of %u KB downloaded)
#define MSG_COMPLETED %d%% completed
#define MSG_REQUIRED (Required)
#define MSG_CANCELING Canceling Setup, please wait...
#define MSG_ERROR_UNCOMPRESS Error uncompressing file %s: %d
#define MSG_USAGE Usage: %s [options]\n	[options] can be any of the following combination:\n		-h: This help.\n		-a [path]: Alternate archive search path.\n		-app [app id]: ID of application which is launching the installer (shared installs)\n		-app_path [app]: Points to (full path) representative file of application (Shared installs)\n	*	-dd [path]: Suggested install destination directory. (Shared installs)\n	*	-greLocal: Forces GRE to be installed into the application dir.\n	*	-greShared: Forces GRE to be installed into a global, shared dir (normally)\n			c:\program files\common files\mozilla.org\GRE\n		-reg_path [path]: Where to make entries in the Windows registry. (Shared installs)\n		-f: Force install of GRE installer (Shared installs), though it'll work\n			for non GRE installers too.\n		-greForce: Force 'Component GRE' to be downloaded, run, and installed.  This\n			bypasses GRE's logic of determining when to install by running its\n			installer with a '-f' flag.\n		-n [filename]: Setup's parent process filename.\n	*	-ma: Run setup in Auto mode.\n	*	-ms: Run setup in Silent mode.\n		-ira: Ignore the [RunAppX] sections\n		-ispf: Ignore the [Program FolderX] sections that show\n			the Start Menu shortcut folder at the end of installation.\n	*	-showBanner: Show the banner image in the download and install progress dialogs\n	*	-hideBanner: Hide the banner image in the download and install progress dialogs\n	*	-cleanupOnUpgrade: Tells Setup to check to see if user is upgrading (installing on top\n		of previous version of product).  If user is upgrading:\n			* NORMAL mode: prompt user on how to proceed\n			* All other modes: assume user wants to cleanup.\n	*	-noCleanupOnUpgrade: Tells Setup to not check if user is upgrading (installing on top\n		of previous version of product).  This will disable the cleanup feature.\n\n	* means it will override config.ini
#define MSG_USE_FTP Use &FTP for downloading files
#define MSG_USE_HTTP Use &HTTP for downloading files
#define MSG_SAVE_LOCALLY &Save installer files locally
#define MSG_DL_PAUSED Setup has encountered a network problem and has paused the download.  If you have just lost your network connection, please click Resume once your network has been reestablished.
#define MSG_NOT_ADMIN Setup has detected that you do not have the appropriate Administrator privileges to this system.  It is highly recommended that you do not continue with the installation of $ProductShortName$, or it may not function properly.  Would you still like to continue?
#define MSG_NOT_ADMIN_AUTO Setup has detected that you do not have the appropriate Administrator privileges to this system.  Setup cannot continue with the installation of $ProductShortName$.

#define UNINST_RUNNING $ProductNameInternal$ is detected to be currently running.  Please quit $ProductNameInternal$ before continuing.  Click OK to exit $ProductNameInternal$ automatically and proceed with uninstallation.
#define UNINST_SHUTDOWN Shutting down $ProductNameInternal$.  Please wait...

#define UNINST_FONT MS Sans Serif
#define UNINST_FONTSIZE 8
#define UNINST_CHARSET 0
# Here is a partial list CHAR_SETS
#  ANSI_CHARSET = 0
#  DEFAULT_CHARSET = 1
#  SYMBOL_CHARSET = 2
#  SHIFTJIS_CHARSET = 128
#  GB2312_CHARSET = 134
#  HANGEUL_CHARSET = 129
#  CHINESEBIG5_CHARSET = 136
#  OEM_CHARSET 255

#define UNINST_TITLE $ProductName$ Uninstaller
#define UNINST_MESSAGE0 Are you sure you want to completely remove %s and all of its components?
#define BTN_UNINST &Uninstall
#define BTN_CANCEL &Cancel
#define BTN_MESSAGE1 Uninstall has detected that the following shared file is no longer used by any programs.  If any programs still require the shared file and it is removed, those programs may no longer function.  Are you sure you want to remove this shared file?
#define BTN_MESSAGE2 Leaving this file will not harm your system.  If you are not sure, it is recommended that the shared file be not removed from the system.
#define BTN_FILENAME File name:
#define BTN_NO &No
#define BTN_NOTOALL N&o to all
#define BTN_YES &Yes
#define BTN_YESTOALL Y&es to all

#define ERROR_DLL_LOAD Could not load %s
#define ERROR_STRING_LOAD Could not load string resource ID %d
#define ERROR_STRING_NULL Null pointer encountered.
#define ERROR_GLOBALALLOC Memory allocation error.
#define ERROR_FAILED %s failed.
#define ERROR_DIALOG_CREATE Could not create %s dialog.
#define DLGQUITTITLE Question
#define DLGQUITMSG Are you sure you want to cancel?
#define ERROR_GET_SYSTEM_DIRECTORY_FAILED GetSystemDirectory() failed.
#define ERROR_GET_WINDOWS_DIRECTORY_FAILED GetWindowsDirectory() failed.
#define ERROR_CREATE_TEMP_DIR Uninstall was not able to create the TEMP directory: %s
#define ERROR_SETUP_REQUIREMENT Windows95 or greater Operating System is required!  Exiting Uninstall...
#define MB_WARNING_STR Warning
#define ERROR_UNINSTALL_LOG_FOLDER Uninstall log folder not found:%s
#define MB_MESSAGE_STR Message
#define DLG_REMOVE_FILE_TITLE Remove File?
#define ERROR_GETVERSION GetVersionEx() failed!
#define MB_ATTENTION_STR Attention
#define MSG_FORCE_QUIT_PROCESS Uninstall has detected that %s (%s) is still running.  Click OK to quit %s and proceed with uninstallation. Alternatively, use the Windows Task Manager to quit %s, and then click OK to continue with uninstallation.
#define MSG_FORCE_QUIT_PROCESS_FAILED Uninstall will now exit.  Uninstall could not continue because %s (%s) is still running. Try manually quitting %s using Windows Task Manager, and then run Uninstall again.
#define MSG_DELETE_INSTALLATION_PATH Not all files were uninstalled from the installation directory:\n\n  %s\n\nDo you want to completely delete this directory?
#define MSG_INSTALLATION_PATH_WITHIN_WINDIR Uninstall has detected that the installation path of $ProductNameInternal$ is installed to a folder within your Windows folder.  Uninstall will not attempt to delete this installation due to the potential removal of critical system files.

#unfilter emptyLines
