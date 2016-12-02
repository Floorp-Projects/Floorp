/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This header file defines all DOM key name which are used for DOM
 * KeyboardEvent.key.
 * You must define NS_DEFINE_KEYNAME macro before including this.
 *
 * It must have two arguments, (aCPPName, aDOMKeyName)
 * aCPPName is usable name for a part of C++ constants.
 * aDOMKeyName is the actual value.
 */

#define DEFINE_KEYNAME_INTERNAL(aCPPName, aDOMKeyName) \
  NS_DEFINE_KEYNAME(aCPPName, aDOMKeyName)

#define DEFINE_KEYNAME_WITH_SAME_NAME(aName) \
  DEFINE_KEYNAME_INTERNAL(aName, #aName)

/******************************************************************************
 * Special Key Values
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(Unidentified)

/******************************************************************************
 * Our Internal Key Values (must have "Moz" prefix)
 *****************************************************************************/
DEFINE_KEYNAME_INTERNAL(PrintableKey, "MozPrintableKey")
DEFINE_KEYNAME_INTERNAL(SoftLeft, "MozSoftLeft")
DEFINE_KEYNAME_INTERNAL(SoftRight, "MozSoftRight")

#ifdef MOZ_B2G
DEFINE_KEYNAME_INTERNAL(HomeScreen, "MozHomeScreen")
#endif // #ifdef MOZ_B2G

/******************************************************************************
 * Modifier Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(Alt)
DEFINE_KEYNAME_WITH_SAME_NAME(AltGraph)
DEFINE_KEYNAME_WITH_SAME_NAME(CapsLock)
DEFINE_KEYNAME_WITH_SAME_NAME(Control)
DEFINE_KEYNAME_WITH_SAME_NAME(Fn)
DEFINE_KEYNAME_WITH_SAME_NAME(FnLock)
DEFINE_KEYNAME_WITH_SAME_NAME(Hyper)
DEFINE_KEYNAME_WITH_SAME_NAME(Meta)
DEFINE_KEYNAME_WITH_SAME_NAME(NumLock)
DEFINE_KEYNAME_WITH_SAME_NAME(OS) // Dropped from the latest draft, bug 1232918
DEFINE_KEYNAME_WITH_SAME_NAME(ScrollLock)
DEFINE_KEYNAME_WITH_SAME_NAME(Shift)
DEFINE_KEYNAME_WITH_SAME_NAME(Super)
DEFINE_KEYNAME_WITH_SAME_NAME(Symbol)
DEFINE_KEYNAME_WITH_SAME_NAME(SymbolLock)

/******************************************************************************
 * Whitespace Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(Enter)
DEFINE_KEYNAME_WITH_SAME_NAME(Tab)

/******************************************************************************
 * Navigation Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(ArrowDown)
DEFINE_KEYNAME_WITH_SAME_NAME(ArrowLeft)
DEFINE_KEYNAME_WITH_SAME_NAME(ArrowRight)
DEFINE_KEYNAME_WITH_SAME_NAME(ArrowUp)
DEFINE_KEYNAME_WITH_SAME_NAME(End)
DEFINE_KEYNAME_WITH_SAME_NAME(Home)
DEFINE_KEYNAME_WITH_SAME_NAME(PageDown)
DEFINE_KEYNAME_WITH_SAME_NAME(PageUp)

/******************************************************************************
 * Editing Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(Backspace)
DEFINE_KEYNAME_WITH_SAME_NAME(Clear)
DEFINE_KEYNAME_WITH_SAME_NAME(Copy)
DEFINE_KEYNAME_WITH_SAME_NAME(CrSel)
DEFINE_KEYNAME_WITH_SAME_NAME(Cut)
DEFINE_KEYNAME_WITH_SAME_NAME(Delete)
DEFINE_KEYNAME_WITH_SAME_NAME(EraseEof)
DEFINE_KEYNAME_WITH_SAME_NAME(ExSel)
DEFINE_KEYNAME_WITH_SAME_NAME(Insert)
DEFINE_KEYNAME_WITH_SAME_NAME(Paste)
DEFINE_KEYNAME_WITH_SAME_NAME(Redo)
DEFINE_KEYNAME_WITH_SAME_NAME(Undo)

/******************************************************************************
 * UI Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(Accept)
DEFINE_KEYNAME_WITH_SAME_NAME(Again)
DEFINE_KEYNAME_WITH_SAME_NAME(Attn)
DEFINE_KEYNAME_WITH_SAME_NAME(Cancel)
DEFINE_KEYNAME_WITH_SAME_NAME(ContextMenu)
DEFINE_KEYNAME_WITH_SAME_NAME(Escape)
DEFINE_KEYNAME_WITH_SAME_NAME(Execute)
DEFINE_KEYNAME_WITH_SAME_NAME(Find)
DEFINE_KEYNAME_WITH_SAME_NAME(Help)
DEFINE_KEYNAME_WITH_SAME_NAME(Pause)
DEFINE_KEYNAME_WITH_SAME_NAME(Play)
DEFINE_KEYNAME_WITH_SAME_NAME(Props)
DEFINE_KEYNAME_WITH_SAME_NAME(Select)
DEFINE_KEYNAME_WITH_SAME_NAME(ZoomIn)
DEFINE_KEYNAME_WITH_SAME_NAME(ZoomOut)

/******************************************************************************
 * Device Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(BrightnessDown)
DEFINE_KEYNAME_WITH_SAME_NAME(BrightnessUp)
DEFINE_KEYNAME_WITH_SAME_NAME(Eject)
DEFINE_KEYNAME_WITH_SAME_NAME(LogOff)
DEFINE_KEYNAME_WITH_SAME_NAME(Power)
DEFINE_KEYNAME_WITH_SAME_NAME(PowerOff)
DEFINE_KEYNAME_WITH_SAME_NAME(PrintScreen)
DEFINE_KEYNAME_WITH_SAME_NAME(Hibernate)
DEFINE_KEYNAME_WITH_SAME_NAME(Standby)
DEFINE_KEYNAME_WITH_SAME_NAME(WakeUp)

/******************************************************************************
 * IME and Composition Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(AllCandidates)
DEFINE_KEYNAME_WITH_SAME_NAME(Alphanumeric)
DEFINE_KEYNAME_WITH_SAME_NAME(CodeInput)
DEFINE_KEYNAME_WITH_SAME_NAME(Compose)
DEFINE_KEYNAME_WITH_SAME_NAME(Convert)
DEFINE_KEYNAME_WITH_SAME_NAME(Dead)
DEFINE_KEYNAME_WITH_SAME_NAME(FinalMode)
DEFINE_KEYNAME_WITH_SAME_NAME(GroupFirst)
DEFINE_KEYNAME_WITH_SAME_NAME(GroupLast)
DEFINE_KEYNAME_WITH_SAME_NAME(GroupNext)
DEFINE_KEYNAME_WITH_SAME_NAME(GroupPrevious)
DEFINE_KEYNAME_WITH_SAME_NAME(ModeChange)
DEFINE_KEYNAME_WITH_SAME_NAME(NextCandidate)
DEFINE_KEYNAME_WITH_SAME_NAME(NonConvert)
DEFINE_KEYNAME_WITH_SAME_NAME(PreviousCandidate)
DEFINE_KEYNAME_WITH_SAME_NAME(Process)
DEFINE_KEYNAME_WITH_SAME_NAME(SingleCandidate)

/******************************************************************************
 * Keys specific to Korean keyboards
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(HangulMode)
DEFINE_KEYNAME_WITH_SAME_NAME(HanjaMode)
DEFINE_KEYNAME_WITH_SAME_NAME(JunjaMode)

/******************************************************************************
 * Keys specific to Japanese keyboards
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(Eisu)
DEFINE_KEYNAME_WITH_SAME_NAME(Hankaku)
DEFINE_KEYNAME_WITH_SAME_NAME(Hiragana)
DEFINE_KEYNAME_WITH_SAME_NAME(HiraganaKatakana)
DEFINE_KEYNAME_WITH_SAME_NAME(KanaMode)
DEFINE_KEYNAME_WITH_SAME_NAME(KanjiMode)
DEFINE_KEYNAME_WITH_SAME_NAME(Katakana)
DEFINE_KEYNAME_WITH_SAME_NAME(Romaji)
DEFINE_KEYNAME_WITH_SAME_NAME(Zenkaku)
DEFINE_KEYNAME_WITH_SAME_NAME(ZenkakuHankaku)

/******************************************************************************
 * General-Purpose Function Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(F1)
DEFINE_KEYNAME_WITH_SAME_NAME(F2)
DEFINE_KEYNAME_WITH_SAME_NAME(F3)
DEFINE_KEYNAME_WITH_SAME_NAME(F4)
DEFINE_KEYNAME_WITH_SAME_NAME(F5)
DEFINE_KEYNAME_WITH_SAME_NAME(F6)
DEFINE_KEYNAME_WITH_SAME_NAME(F7)
DEFINE_KEYNAME_WITH_SAME_NAME(F8)
DEFINE_KEYNAME_WITH_SAME_NAME(F9)
DEFINE_KEYNAME_WITH_SAME_NAME(F10)
DEFINE_KEYNAME_WITH_SAME_NAME(F11)
DEFINE_KEYNAME_WITH_SAME_NAME(F12)
DEFINE_KEYNAME_WITH_SAME_NAME(F13)
DEFINE_KEYNAME_WITH_SAME_NAME(F14)
DEFINE_KEYNAME_WITH_SAME_NAME(F15)
DEFINE_KEYNAME_WITH_SAME_NAME(F16)
DEFINE_KEYNAME_WITH_SAME_NAME(F17)
DEFINE_KEYNAME_WITH_SAME_NAME(F18)
DEFINE_KEYNAME_WITH_SAME_NAME(F19)
DEFINE_KEYNAME_WITH_SAME_NAME(F20)
DEFINE_KEYNAME_WITH_SAME_NAME(F21)
DEFINE_KEYNAME_WITH_SAME_NAME(F22)
DEFINE_KEYNAME_WITH_SAME_NAME(F23)
DEFINE_KEYNAME_WITH_SAME_NAME(F24)
DEFINE_KEYNAME_WITH_SAME_NAME(F25)
DEFINE_KEYNAME_WITH_SAME_NAME(F26)
DEFINE_KEYNAME_WITH_SAME_NAME(F27)
DEFINE_KEYNAME_WITH_SAME_NAME(F28)
DEFINE_KEYNAME_WITH_SAME_NAME(F29)
DEFINE_KEYNAME_WITH_SAME_NAME(F30)
DEFINE_KEYNAME_WITH_SAME_NAME(F31)
DEFINE_KEYNAME_WITH_SAME_NAME(F32)
DEFINE_KEYNAME_WITH_SAME_NAME(F33)
DEFINE_KEYNAME_WITH_SAME_NAME(F34)
DEFINE_KEYNAME_WITH_SAME_NAME(F35)
DEFINE_KEYNAME_WITH_SAME_NAME(Soft1)
DEFINE_KEYNAME_WITH_SAME_NAME(Soft2)
DEFINE_KEYNAME_WITH_SAME_NAME(Soft3)
DEFINE_KEYNAME_WITH_SAME_NAME(Soft4)

/******************************************************************************
 * Multimedia Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(ChannelDown)
DEFINE_KEYNAME_WITH_SAME_NAME(ChannelUp)
DEFINE_KEYNAME_WITH_SAME_NAME(Close)
DEFINE_KEYNAME_WITH_SAME_NAME(MailForward)
DEFINE_KEYNAME_WITH_SAME_NAME(MailReply)
DEFINE_KEYNAME_WITH_SAME_NAME(MailSend)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaFastForward)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaPause)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaPlay)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaPlayPause)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaRecord)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaRewind)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaStop)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaTrackNext)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaTrackPrevious)
DEFINE_KEYNAME_WITH_SAME_NAME(New)
DEFINE_KEYNAME_WITH_SAME_NAME(Open)
DEFINE_KEYNAME_WITH_SAME_NAME(Print)
DEFINE_KEYNAME_WITH_SAME_NAME(Save)
DEFINE_KEYNAME_WITH_SAME_NAME(SpellCheck)

/******************************************************************************
 * Multimedia Numpad Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(Key11)
DEFINE_KEYNAME_WITH_SAME_NAME(Key12)

/******************************************************************************
 * Audio Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(AudioBalanceLeft)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioBalanceRight)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioBassBoostDown)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioBassBoostToggle)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioBassBoostUp)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioFaderFront)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioFaderRear)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioSurroundModeNext)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioTrebleDown)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioTrebleUp)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioVolumeDown)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioVolumeUp)
DEFINE_KEYNAME_WITH_SAME_NAME(AudioVolumeMute)

DEFINE_KEYNAME_WITH_SAME_NAME(MicrophoneToggle)
DEFINE_KEYNAME_WITH_SAME_NAME(MicrophoneVolumeDown)
DEFINE_KEYNAME_WITH_SAME_NAME(MicrophoneVolumeUp)
DEFINE_KEYNAME_WITH_SAME_NAME(MicrophoneVolumeMute)

/******************************************************************************
 * Speech Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(SpeechCorrectionList)
DEFINE_KEYNAME_WITH_SAME_NAME(SpeechInputToggle)

/******************************************************************************
 * Application Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchCalculator)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchCalendar)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchContacts)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchMail)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchMediaPlayer)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchMusicPlayer)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchMyComputer)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchPhone)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchScreenSaver)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchSpreadsheet)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchWebBrowser)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchWebCam)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchWordProcessor)

DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication1)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication2)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication3)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication4)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication5)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication6)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication7)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication8)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication9)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication10)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication11)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication12)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication13)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication14)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication15)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication16)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication17)
DEFINE_KEYNAME_WITH_SAME_NAME(LaunchApplication18)

/******************************************************************************
 * Browser Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(BrowserBack)
DEFINE_KEYNAME_WITH_SAME_NAME(BrowserFavorites)
DEFINE_KEYNAME_WITH_SAME_NAME(BrowserForward)
DEFINE_KEYNAME_WITH_SAME_NAME(BrowserHome)
DEFINE_KEYNAME_WITH_SAME_NAME(BrowserRefresh)
DEFINE_KEYNAME_WITH_SAME_NAME(BrowserSearch)
DEFINE_KEYNAME_WITH_SAME_NAME(BrowserStop)

/******************************************************************************
 * Mobile Phone Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(AppSwitch)
DEFINE_KEYNAME_WITH_SAME_NAME(Call)
DEFINE_KEYNAME_WITH_SAME_NAME(Camera)
DEFINE_KEYNAME_WITH_SAME_NAME(CameraFocus)
DEFINE_KEYNAME_WITH_SAME_NAME(EndCall)
DEFINE_KEYNAME_WITH_SAME_NAME(GoBack)
DEFINE_KEYNAME_WITH_SAME_NAME(GoHome)
DEFINE_KEYNAME_WITH_SAME_NAME(HeadsetHook)
DEFINE_KEYNAME_WITH_SAME_NAME(LastNumberRedial)
DEFINE_KEYNAME_WITH_SAME_NAME(Notification)
DEFINE_KEYNAME_WITH_SAME_NAME(MannerMode)
DEFINE_KEYNAME_WITH_SAME_NAME(VoiceDial)

/******************************************************************************
 * TV Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(TV)
DEFINE_KEYNAME_WITH_SAME_NAME(TV3DMode)
DEFINE_KEYNAME_WITH_SAME_NAME(TVAntennaCable)
DEFINE_KEYNAME_WITH_SAME_NAME(TVAudioDescription)
DEFINE_KEYNAME_WITH_SAME_NAME(TVAudioDescriptionMixDown)
DEFINE_KEYNAME_WITH_SAME_NAME(TVAudioDescriptionMixUp)
DEFINE_KEYNAME_WITH_SAME_NAME(TVContentsMenu)
DEFINE_KEYNAME_WITH_SAME_NAME(TVDataService)
DEFINE_KEYNAME_WITH_SAME_NAME(TVInput)
DEFINE_KEYNAME_WITH_SAME_NAME(TVInputComponent1)
DEFINE_KEYNAME_WITH_SAME_NAME(TVInputComponent2)
DEFINE_KEYNAME_WITH_SAME_NAME(TVInputComposite1)
DEFINE_KEYNAME_WITH_SAME_NAME(TVInputComposite2)
DEFINE_KEYNAME_WITH_SAME_NAME(TVInputHDMI1)
DEFINE_KEYNAME_WITH_SAME_NAME(TVInputHDMI2)
DEFINE_KEYNAME_WITH_SAME_NAME(TVInputHDMI3)
DEFINE_KEYNAME_WITH_SAME_NAME(TVInputHDMI4)
DEFINE_KEYNAME_WITH_SAME_NAME(TVInputVGA1)
DEFINE_KEYNAME_WITH_SAME_NAME(TVMediaContext)
DEFINE_KEYNAME_WITH_SAME_NAME(TVNetwork)
DEFINE_KEYNAME_WITH_SAME_NAME(TVNumberEntry)
DEFINE_KEYNAME_WITH_SAME_NAME(TVPower)
DEFINE_KEYNAME_WITH_SAME_NAME(TVRadioService)
DEFINE_KEYNAME_WITH_SAME_NAME(TVSatellite)
DEFINE_KEYNAME_WITH_SAME_NAME(TVSatelliteBS)
DEFINE_KEYNAME_WITH_SAME_NAME(TVSatelliteCS)
DEFINE_KEYNAME_WITH_SAME_NAME(TVSatelliteToggle)
DEFINE_KEYNAME_WITH_SAME_NAME(TVTerrestrialAnalog)
DEFINE_KEYNAME_WITH_SAME_NAME(TVTerrestrialDigital)
DEFINE_KEYNAME_WITH_SAME_NAME(TVTimer)

/******************************************************************************
 * Media Controller Keys
 *****************************************************************************/
DEFINE_KEYNAME_WITH_SAME_NAME(AVRInput)
DEFINE_KEYNAME_WITH_SAME_NAME(AVRPower)
DEFINE_KEYNAME_WITH_SAME_NAME(ColorF0Red)
DEFINE_KEYNAME_WITH_SAME_NAME(ColorF1Green)
DEFINE_KEYNAME_WITH_SAME_NAME(ColorF2Yellow)
DEFINE_KEYNAME_WITH_SAME_NAME(ColorF3Blue)
DEFINE_KEYNAME_WITH_SAME_NAME(ColorF4Grey)
DEFINE_KEYNAME_WITH_SAME_NAME(ColorF5Brown)
DEFINE_KEYNAME_WITH_SAME_NAME(ClosedCaptionToggle)
DEFINE_KEYNAME_WITH_SAME_NAME(Dimmer)
DEFINE_KEYNAME_WITH_SAME_NAME(DisplaySwap)
DEFINE_KEYNAME_WITH_SAME_NAME(DVR)
DEFINE_KEYNAME_WITH_SAME_NAME(Exit)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteClear0)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteClear1)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteClear2)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteClear3)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteRecall0)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteRecall1)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteRecall2)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteRecall3)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteStore0)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteStore1)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteStore2)
DEFINE_KEYNAME_WITH_SAME_NAME(FavoriteStore3)
DEFINE_KEYNAME_WITH_SAME_NAME(Guide)
DEFINE_KEYNAME_WITH_SAME_NAME(GuideNextDay)
DEFINE_KEYNAME_WITH_SAME_NAME(GuidePreviousDay)
DEFINE_KEYNAME_WITH_SAME_NAME(Info)
DEFINE_KEYNAME_WITH_SAME_NAME(InstantReplay)
DEFINE_KEYNAME_WITH_SAME_NAME(Link)
DEFINE_KEYNAME_WITH_SAME_NAME(ListProgram)
DEFINE_KEYNAME_WITH_SAME_NAME(LiveContent)
DEFINE_KEYNAME_WITH_SAME_NAME(Lock)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaApps)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaAudioTrack)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaLast)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaSkipBackward)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaSkipForward)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaStepBackward)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaStepForward)
DEFINE_KEYNAME_WITH_SAME_NAME(MediaTopMenu)
DEFINE_KEYNAME_WITH_SAME_NAME(NavigateIn)
DEFINE_KEYNAME_WITH_SAME_NAME(NavigateNext)
DEFINE_KEYNAME_WITH_SAME_NAME(NavigateOut)
DEFINE_KEYNAME_WITH_SAME_NAME(NavigatePrevious)
DEFINE_KEYNAME_WITH_SAME_NAME(NextFavoriteChannel)
DEFINE_KEYNAME_WITH_SAME_NAME(NextUserProfile)
DEFINE_KEYNAME_WITH_SAME_NAME(OnDemand)
DEFINE_KEYNAME_WITH_SAME_NAME(Pairing)
DEFINE_KEYNAME_WITH_SAME_NAME(PinPDown)
DEFINE_KEYNAME_WITH_SAME_NAME(PinPMove)
DEFINE_KEYNAME_WITH_SAME_NAME(PinPToggle)
DEFINE_KEYNAME_WITH_SAME_NAME(PinPUp)
DEFINE_KEYNAME_WITH_SAME_NAME(PlaySpeedDown)
DEFINE_KEYNAME_WITH_SAME_NAME(PlaySpeedReset)
DEFINE_KEYNAME_WITH_SAME_NAME(PlaySpeedUp)
DEFINE_KEYNAME_WITH_SAME_NAME(RandomToggle)
DEFINE_KEYNAME_WITH_SAME_NAME(RcLowBattery)
DEFINE_KEYNAME_WITH_SAME_NAME(RecordSpeedNext)
DEFINE_KEYNAME_WITH_SAME_NAME(RfBypass)
DEFINE_KEYNAME_WITH_SAME_NAME(ScanChannelsToggle)
DEFINE_KEYNAME_WITH_SAME_NAME(ScreenModeNext)
DEFINE_KEYNAME_WITH_SAME_NAME(Settings)
DEFINE_KEYNAME_WITH_SAME_NAME(SplitScreenToggle)
DEFINE_KEYNAME_WITH_SAME_NAME(STBInput)
DEFINE_KEYNAME_WITH_SAME_NAME(STBPower)
DEFINE_KEYNAME_WITH_SAME_NAME(Subtitle)
DEFINE_KEYNAME_WITH_SAME_NAME(Teletext)
DEFINE_KEYNAME_WITH_SAME_NAME(VideoModeNext)
DEFINE_KEYNAME_WITH_SAME_NAME(Wink)
DEFINE_KEYNAME_WITH_SAME_NAME(ZoomToggle)

#undef DEFINE_KEYNAME_WITH_SAME_NAME
#undef DEFINE_KEYNAME_INTERNAL
