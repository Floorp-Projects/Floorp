VERSION 5.00
Object = "{6B7E6392-850A-101B-AFC0-4210102A8DA7}#1.2#0"; "comctl32.ocx"
Begin VB.Form frmToolBar 
   Caption         =   "Control Bar"
   ClientHeight    =   1215
   ClientLeft      =   165
   ClientTop       =   735
   ClientWidth     =   7965
   LinkTopic       =   "Form2"
   ScaleHeight     =   81
   ScaleMode       =   3  'Pixel
   ScaleWidth      =   531
   StartUpPosition =   3  'Windows Default
   Begin ComctlLib.Toolbar Toolbar1 
      Align           =   1  'Align Top
      Height          =   660
      Left            =   0
      TabIndex        =   0
      Top             =   0
      Width           =   7965
      _ExtentX        =   14049
      _ExtentY        =   1164
      ButtonWidth     =   1032
      ButtonHeight    =   1005
      AllowCustomize  =   0   'False
      Appearance      =   1
      ImageList       =   "ImageList1"
      _Version        =   327682
      BeginProperty Buttons {0713E452-850A-101B-AFC0-4210102A8DA7} 
         NumButtons      =   10
         BeginProperty Button1 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Key             =   "goback"
            Object.ToolTipText     =   "Go Back"
            Object.Tag             =   ""
            ImageIndex      =   1
         EndProperty
         BeginProperty Button2 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Key             =   "goforward"
            Object.ToolTipText     =   "Go Forward"
            Object.Tag             =   ""
            ImageIndex      =   5
         EndProperty
         BeginProperty Button3 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Key             =   "reload"
            Object.ToolTipText     =   "Reload Page"
            Object.Tag             =   ""
            ImageIndex      =   6
         EndProperty
         BeginProperty Button4 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Object.Tag             =   ""
            Style           =   3
            MixedState      =   -1  'True
         EndProperty
         BeginProperty Button5 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Key             =   "gohome"
            Object.ToolTipText     =   "Go Home"
            Object.Tag             =   ""
            ImageIndex      =   3
         EndProperty
         BeginProperty Button6 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Key             =   "gosearch"
            Object.ToolTipText     =   "Search Web"
            Object.Tag             =   ""
            ImageIndex      =   8
         EndProperty
         BeginProperty Button7 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Key             =   "ph"
            Object.Tag             =   ""
            Style           =   4
            Object.Width           =   200
            MixedState      =   -1  'True
         EndProperty
         BeginProperty Button8 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Key             =   "loadpage"
            Object.ToolTipText     =   "Load this URL"
            Object.Tag             =   ""
            ImageIndex      =   4
         EndProperty
         BeginProperty Button9 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Object.Tag             =   ""
            Style           =   3
            MixedState      =   -1  'True
         EndProperty
         BeginProperty Button10 {0713F354-850A-101B-AFC0-4210102A8DA7} 
            Key             =   "stop"
            Object.ToolTipText     =   "Stop Loading"
            Object.Tag             =   ""
            ImageIndex      =   2
         EndProperty
      EndProperty
      Begin VB.ComboBox cmbUrl 
         BeginProperty Font 
            Name            =   "MS Sans Serif"
            Size            =   12
            Charset         =   0
            Weight          =   400
            Underline       =   0   'False
            Italic          =   0   'False
            Strikethrough   =   0   'False
         EndProperty
         Height          =   420
         ItemData        =   "frmToolBar.frx":0000
         Left            =   3120
         List            =   "frmToolBar.frx":0016
         TabIndex        =   1
         Text            =   "http://www.mozilla.com"
         Top             =   120
         Width           =   2895
      End
   End
   Begin VB.OptionButton rbExplorer 
      Caption         =   "InternetExplorer"
      Height          =   255
      Left            =   2160
      TabIndex        =   4
      Top             =   720
      Width           =   1575
   End
   Begin VB.OptionButton rbMozilla 
      Caption         =   "Mozilla"
      Height          =   255
      Left            =   960
      TabIndex        =   3
      Top             =   720
      Value           =   -1  'True
      Width           =   1215
   End
   Begin ComctlLib.StatusBar StatusBar1 
      Align           =   2  'Align Bottom
      Height          =   255
      Left            =   0
      TabIndex        =   2
      Top             =   960
      Width           =   7965
      _ExtentX        =   14049
      _ExtentY        =   450
      SimpleText      =   ""
      _Version        =   327682
      BeginProperty Panels {0713E89E-850A-101B-AFC0-4210102A8DA7} 
         NumPanels       =   2
         BeginProperty Panel1 {0713E89F-850A-101B-AFC0-4210102A8DA7} 
            AutoSize        =   1
            Object.Width           =   11404
            MinWidth        =   2646
            TextSave        =   ""
            Object.Tag             =   ""
         EndProperty
         BeginProperty Panel2 {0713E89F-850A-101B-AFC0-4210102A8DA7} 
            Alignment       =   2
            Object.Width           =   2117
            MinWidth        =   2117
            TextSave        =   ""
            Object.Tag             =   ""
         EndProperty
      EndProperty
   End
   Begin VB.Label Label1 
      Caption         =   "Control:"
      Height          =   255
      Left            =   120
      TabIndex        =   5
      Top             =   720
      Width           =   855
   End
   Begin ComctlLib.ImageList ImageList1 
      Left            =   6240
      Top             =   480
      _ExtentX        =   1005
      _ExtentY        =   1005
      BackColor       =   -2147483643
      ImageWidth      =   32
      ImageHeight     =   32
      MaskColor       =   12632256
      _Version        =   327682
      BeginProperty Images {0713E8C2-850A-101B-AFC0-4210102A8DA7} 
         NumListImages   =   8
         BeginProperty ListImage1 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "frmToolBar.frx":00A4
            Key             =   "back"
         EndProperty
         BeginProperty ListImage2 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "frmToolBar.frx":03BE
            Key             =   "stop"
         EndProperty
         BeginProperty ListImage3 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "frmToolBar.frx":06D8
            Key             =   "home"
         EndProperty
         BeginProperty ListImage4 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "frmToolBar.frx":09F2
            Key             =   "gotopage"
         EndProperty
         BeginProperty ListImage5 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "frmToolBar.frx":0D0C
            Key             =   "forward"
         EndProperty
         BeginProperty ListImage6 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "frmToolBar.frx":1026
            Key             =   "reload"
         EndProperty
         BeginProperty ListImage7 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "frmToolBar.frx":1340
            Key             =   "go"
         EndProperty
         BeginProperty ListImage8 {0713E8C3-850A-101B-AFC0-4210102A8DA7} 
            Picture         =   "frmToolBar.frx":165A
            Key             =   "gofind"
         EndProperty
      EndProperty
   End
   Begin VB.Menu debug 
      Caption         =   "Debug"
      Begin VB.Menu verbs 
         Caption         =   "OLE Verbs"
      End
   End
End
Attribute VB_Name = "frmToolBar"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Dim browser As Object

Private Sub Form_Load()
    frmMozilla.Show
    frmExplorer.Show
    Set browser = frmMozilla.Browser1
End Sub

Sub Browser_BeforeNavigate(ByVal URL As String, ByVal Flags As Long, ByVal TargetFrameName As String, PostData As Variant, ByVal Headers As String, Cancel As Boolean)
    Debug.Print "Browser_BeforeNavigate " & URL
    StatusBar1.Panels(1).Text = "Loading " & URL
End Sub

Sub Browser_NavigateComplete(ByVal URL As String)
    Debug.Print "Browser_NavigateComplete " & URL
    StatusBar1.Panels(1).Text = "Loaded " & URL
    StatusBar1.Panels(2).Text = ""
End Sub

Sub Browser_BeforeNavigate2(ByVal pDisp As Object, URL As Variant, Flags As Variant, TargetFrameName As Variant, PostData As Variant, Headers As Variant, Cancel As Boolean)
    Debug.Print "Browser_BeforeNavigate2 " & URL
    StatusBar1.Panels(1).Text = "Loaded " & URL
    StatusBar1.Panels(2).Text = ""
End Sub

Sub Browser_NavigateComplete2(ByVal pDisp As Object, URL As Variant)
    Debug.Print "Browser_NavigateComplete2 " & URL
    StatusBar1.Panels(1).Text = "Loaded " & URL
    StatusBar1.Panels(2).Text = ""
End Sub

Sub Browser_StatusTextChange(ByVal Text As String)
    Debug.Print "Browser_StatusTextChange " & Text
    StatusBar1.Panels(1).Text = Text
End Sub

Sub Browser_ProgressChange(ByVal Progress As Long, ByVal ProgressMax As Long)
    Dim fProgress As Double
    If Progress = 0 Then
'        fProgress = 0
    ElseIf ProgressMax > 0 Then
'        fProgress = (Progress * 100) / ProgressMax
    Else
        ' fProgress = 0#
        Debug.Print "Progress error - Progress = " & Progress & ", ProgressMax = " & ProgressMax
    End If
'    StatusBar1.Panels(2).Text = Int(fProgress) & "%"
End Sub

Private Sub rbExplorer_Click()
    Set browser = frmExplorer.Browser1
End Sub

Private Sub rbMozilla_Click()
    Set browser = frmMozilla.Browser1
End Sub

Private Sub Toolbar1_ButtonClick(ByVal Button As ComctlLib.Button)
    Select Case Button.Key
    Case "goback"
        browser.GoBack
    Case "goforward"
        browser.GoForward
    Case "reload"
        browser.Refresh
    Case "gohome"
        browser.GoHome
    Case "gosearch"
        browser.GoSearch
    Case "loadpage"
        browser.Navigate cmbUrl.Text
    Case "stop"
        browser.Stop
    Case Else
    End Select
End Sub

Private Sub verbs_Click()
    ' Query the browser to see what IOleCommandTarget commands it supports
    Dim nCmd As Integer
    Dim nStatus As Integer
    For nCmd = 1 To 40
        nStatus = browser.QueryStatusWB(nCmd)
        If nStatus And 1 Then
            Debug.Print "Command " & nCmd & " is supported"
        Else
            Debug.Print "Command " & nCmd & " is not supported"
        End If
        If nStatus And 2 Then
            Debug.Print "Command " & nCmd & " is disabled"
        End If
    Next
End Sub
