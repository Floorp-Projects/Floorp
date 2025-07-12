# Development Environment Setup

## System Requirements

### Minimum System Requirements

#### Windows

- **OS**: Windows 10 (1903) or later, Windows 11
- **Architecture**: x64, ARM64
- **Memory**: 8GB RAM (16GB recommended)
- **Storage**: 10GB or more free space
- **Network**: Stable internet connection for Firefox binary download

#### macOS

- **OS**: macOS 10.15 (Catalina) or later
- **Architecture**: Intel x64, Apple Silicon (M1/M2)
- **Memory**: 8GB RAM (16GB recommended)
- **Storage**: 10GB or more free space
- **Network**: Stable internet connection for Firefox binary download

#### Linux

- **Distribution**: Ubuntu 20.04+, Debian 11+, Fedora 35+, or equivalent
- **Architecture**: x64, ARM64
- **Memory**: 8GB RAM (16GB recommended)
- **Storage**: 10GB or more free space
- **Network**: Stable internet connection for Firefox binary download

## Required Tools

### 1. Deno (Required)

The main runtime for Floorp's build system.

#### Installation

```bash
# Using installation script (Windows/macOS/Linux)
curl -fsSL https://deno.land/install.sh | sh

# Windows (using PowerShell)
irm https://deno.land/install.ps1 | iex

# macOS (using Homebrew)
brew install deno

# Linux (using Snap)
sudo snap install deno

# Verify installation
deno --version
```

#### Required Version

- **Minimum**: Deno 1.40.0 or later
- **Recommended**: Latest stable version

### 2. Node.js (Required)

Used for frontend development tools and package management.

#### Installation

```bash
# Download from official website
# https://nodejs.org/

# Node Version Manager (recommended)
# First install nvm
curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.0/install.sh | bash

# Install and use Node.js
nvm install 20
nvm use 20

# Verify installation
node --version
npm --version
```

#### Required Version

- **Node.js**: 18.0.0 or later (20.x recommended)
- **npm**: 9.0.0 or later

### 3. Git (Required)

Used for version control and patch management.

#### Installation

```bash
# Windows
# Download from https://git-scm.com/

# macOS
brew install git

# Linux (Ubuntu/Debian)
sudo apt install git

# Verify installation
git --version
```

## Step-by-Step Setup

### 1. Clone Repository

```bash
# Clone repository
git clone https://github.com/Floorp-Projects/Floorp.git -b main
cd Floorp

# If you have SSH access
git clone git@github.com:Floorp-Projects/Floorp.git -b main
cd Floorp
```

### 4. Windows Only (PowerShell Installation)

PowerShell is required to launch Floorp.

```powershell
# Install PowerShell
winget install --id Microsoft.PowerShell --source winget
```

### 2. Install Dependencies

```bash
deno install
```

### 3. Initial Build

```bash
# Initial build (downloads Firefox binary)
deno task build

# This process may take 10-30 minutes depending on network speed
# Firefox binary (approximately 200-500MB) will be downloaded and extracted
```

### 4. Start Development Server

```bash
# Launch in development mode
deno task dev

# Browser will launch automatically
# Hot reload is enabled for code changes
```

## Development Tool Configuration

### Visual Studio Code

#### Recommended Extensions

```json
{
  "recommendations": [
    "denoland.vscode-deno",
    "bradlc.vscode-tailwindcss",
    "ms-vscode.vscode-typescript-next",
    "esbenp.prettier-vscode",
    "dbaeumer.vscode-eslint"
  ]
}
```

#### Settings Example

Create `.vscode/settings.json`:

```json
{
  "deno.enable": true,
  "deno.lint": true,
  "deno.unstable": true,
  "typescript.preferences.importModuleSpecifier": "relative",
  "editor.defaultFormatter": "denoland.vscode-deno",
  "[typescript]": {
    "editor.defaultFormatter": "denoland.vscode-deno"
  },
  "[javascript]": {
    "editor.defaultFormatter": "denoland.vscode-deno"
  }
}
```

#### Task Configuration Example

Create `.vscode/tasks.json`:

```json
{
  "version": "2.0.0",
  "tasks": [
    {
      "label": "Floorp: Dev",
      "type": "shell",
      "command": "deno",
      "args": ["task", "dev"],
      "group": "build",
      "presentation": {
        "echo": true,
        "reveal": "always",
        "focus": false,
        "panel": "shared"
      },
      "problemMatcher": []
    },
    {
      "label": "Floorp: Build",
      "type": "shell",
      "command": "deno",
      "args": ["task", "build"],
      "group": "build",
      "presentation": {
        "echo": true,
        "reveal": "always",
        "focus": false,
        "panel": "shared"
      },
      "problemMatcher": []
    }
  ]
}
```

### Neovim Configuration Example

#### Using lazy.nvim

```lua
return {
  -- Deno support
  {
    "neovim/nvim-lspconfig",
    opts = {
      servers = {
        denols = {
          root_dir = require("lspconfig.util").root_pattern("deno.json", "deno.jsonc"),
          settings = {
            deno = {
              enable = true,
              lint = true,
              unstable = true,
            },
          },
        },
      },
    },
  },

  -- TypeScript support
  {
    "jose-elias-alvarez/typescript.nvim",
    dependencies = "neovim/nvim-lspconfig",
    opts = {},
  },
}
```

### Vim Configuration Example

#### Basic Configuration

```vim
" Deno support
let g:deno_enable = v:true
let g:deno_lint = v:true
let g:deno_unstable = v:true

" TypeScript/JavaScript settings
autocmd FileType typescript,javascript setlocal expandtab shiftwidth=2 tabstop=2

" Auto-format on save
autocmd BufWritePre *.ts,*.js,*.tsx,*.jsx :!deno fmt %
```

## Platform-Specific Configuration

### Windows Configuration

#### PowerShell Execution Policy

```powershell
# Allow script execution (run as administrator)
Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
```

#### Windows Defender Exclusion Settings

For performance improvement, add the following directories to Windows Defender exclusions. However, using Dev Drive is strongly recommended.

- Project root directory
- `_dist/` directory
- Node.js installation directory
- Deno installation directory

### macOS Configuration

#### Xcode Command Line Tools

```bash
# Install Xcode command line tools
xcode-select --install
```

## Troubleshooting

### Issue: Deno not found

```bash
# Solution: Add Deno to PATH
echo 'export PATH="$HOME/.deno/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc
```

### Issue: Permission denied when running scripts

```bash
# Solution: Grant execution permission to scripts
chmod +x build.ts
# Or explicitly grant permission and run
deno run -A build.ts
```

### Issue: Port-related errors on Windows

In that case, enable developer mode in Windows settings and restart your PC. Then run `deno task dev`.

## Next Steps

After setup completion:

1. **Read Architecture Documentation**: Understand Floorp's structure
2. **Explore Codebase**: Start with main app from `src/apps/main/`
3. **Make Your First Changes**: Modify UI components and experience hot reload
4. **Join the Community**: Connect with other developers on Discord or GitHub Discussions

## Getting Help

If you encounter issues during setup:

1. **Check Logs**: Build and development server logs often contain useful error messages
2. **Search Existing Issues**: Look for similar problems in GitHub Issues
3. **Ask Questions**: Ask questions on GitHub Discussions or Discord
4. **Update Tools**: Ensure all required tools are up to date

Run `deno task dev` and if the Floorp browser launches with hot reload, your development environment setup is complete.
